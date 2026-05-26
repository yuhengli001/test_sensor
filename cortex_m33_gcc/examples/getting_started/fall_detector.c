// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#include "fall_detector.h"

#include <math.h>
#include <stdio.h>

#include "acc_definitions_a121.h"
#include "acc_integration_log.h"


app_config_t global_config = {
	// Impact detection sensitivity.
	// Raise to reduce false triggers from sudden non-fall movements (e.g. dropping objects).
	// Lower to catch slower or softer falls. Typical range: 15.0 – 30.0.
	.fall_score_threshold = 20.0f,

	// Minimum distance change from the pre-fall position to count a frame as "fallen" (meters).
	// When the subject was already resting before impact, 2× this value is required
	// to avoid false alarms from bed rolls or posture shifts.
	// Increase if sensor geometry means sitting-down causes a large distance shift.
	// Typical range: 0.15 – 0.40.
	.fall_dist_threshold = 0.20f,

	// How many seconds the subject must remain at the fallen position to confirm a fall.
	// Subject does NOT need to be still during this window — struggling counts.
	// Lower values respond faster but risk false alarms; higher values are more conservative.
	// Typical range: 3 – 10.
	.confirm_period_sec = 5,

	.enable_fall_detection    = true,
	.enable_vitals_monitoring = true,
};

system_mode_t sys_mode = MODE_NORMAL;

// --- MODE_NORMAL state ---
static uint32_t impact_frame_cnt          = 0;
static uint32_t resting_frame_cnt         = 0;
static float    pre_fall_dist             = 0.0f;
static bool     was_resting_before_impact = false;

// --- MODE_SUSPECTED state ---
// Confirmation: how many frames the subject has been at the fallen position
static uint32_t fallen_pos_cnt    = 0;
// Recovery: consecutive frames where subject has returned near the original position
static uint32_t recovery_cnt      = 0;
// Cancel: consecutive frames with very strong motion (subject clearly getting up)
static uint32_t strong_motion_cnt = 0;
// Timeout: total frames spent in MODE_SUSPECTED
static uint32_t suspected_total   = 0;

// --- MODE_ALARM state ---
static uint32_t alarm_print_counter = 0;

// Internal thresholds for MODE_SUSPECTED.
// These are less likely to need tuning than global_config, but can be adjusted here.

// Seconds the subject must be back near the original position to cancel the suspected state.
// Lower = faster cancel when person recovers; higher = more tolerant of brief recoveries.
// Typical range: 1 – 4.
#define RECOVERY_SEC         2.0f
#define RECOVERY_MIN_FRAMES  ((uint32_t)(RECOVERY_SEC * SAMPLE_RATE_HZ))

// intra_score level that indicates the subject is clearly standing up and walking away.
// Must be higher than fall_score_threshold to avoid cancelling on floor-level struggling.
// Typical range: 23.0 – 35.0.
#define CANCEL_SCORE_THR     25.0f

// Seconds of sustained CANCEL_SCORE_THR motion required to cancel.
// Prevents a single energetic movement (e.g. trying to get up) from clearing the alarm.
// Typical range: 0.5 – 2.0.
#define CANCEL_SEC           1.0f
#define CANCEL_MIN_FRAMES    ((uint32_t)(CANCEL_SEC * SAMPLE_RATE_HZ))

// Maximum seconds allowed in MODE_SUSPECTED before giving up and returning to NORMAL.
// Acts as a safety net in case distance tracking produces no clear result.
// Typical range: 20 – 60.
#define SUSPECTED_TIMEOUT_SEC  30.0f
#define SUSPECTED_TIMEOUT      ((uint32_t)(SUSPECTED_TIMEOUT_SEC * SAMPLE_RATE_HZ))


void fall_detector_init(void)
{
	sys_mode             = MODE_NORMAL;
	impact_frame_cnt     = 0;
	resting_frame_cnt    = 0;
	fallen_pos_cnt       = 0;
	recovery_cnt         = 0;
	strong_motion_cnt    = 0;
	suspected_total      = 0;
	alarm_print_counter  = 0;
}


void fall_detector_reset_alarm(void)
{
	if (sys_mode == MODE_ALARM)
	{
		sys_mode            = MODE_NORMAL;
		impact_frame_cnt    = 0;
		fallen_pos_cnt      = 0;
		recovery_cnt        = 0;
		strong_motion_cnt   = 0;
		suspected_total     = 0;
		alarm_print_counter = 0;
		printf("[FALL] Alarm cleared\n");
	}
}


fall_status_t process_fall_detection(float intra_score, float current_dist)
{
	if (!global_config.enable_fall_detection)
	{
		return FALL_STATUS_NORMAL;
	}

	switch (sys_mode)
	{
		case MODE_NORMAL:
		{
			// Track continuous resting state (used to set a stricter distance threshold
			// when the subject was already lying/sitting before the impact)
			if (intra_score < 5.0f)
			{
				resting_frame_cnt++;
			}
			else if (intra_score > 10.0f)
			{
				resting_frame_cnt = 0;
			}

			// Impact detection: sustained energy burst above threshold
			if (intra_score > global_config.fall_score_threshold)
			{
				if (impact_frame_cnt == 0)
				{
					pre_fall_dist = current_dist;
					was_resting_before_impact =
					    (resting_frame_cnt > (uint32_t)(5.0f * SAMPLE_RATE_HZ));
				}

				impact_frame_cnt++;

				if (impact_frame_cnt >= (uint32_t)(0.5f * SAMPLE_RATE_HZ))
				{
					sys_mode          = MODE_SUSPECTED;
					fallen_pos_cnt    = 0;
					recovery_cnt      = 0;
					strong_motion_cnt = 0;
					suspected_total   = 0;
					printf("\n[FALL] Suspected impact at %.2f m\n", pre_fall_dist);
					return FALL_STATUS_SUSPECTED;
				}

				return FALL_STATUS_IMPACT;
			}
			else
			{
				// Tolerate brief single-frame dropouts: decrement rather than reset
				impact_frame_cnt = (impact_frame_cnt > 2) ? impact_frame_cnt - 2 : 0;
			}

			return FALL_STATUS_NORMAL;
		}

		case MODE_SUSPECTED:
		{
			suspected_total++;

			// --- Distance-based confirmation ---
			// The subject does NOT need to be still. As long as they remain at a
			// significantly different position from before the impact, the fall counter
			// continues to accumulate. Struggling, breathing, and movement all allowed.

			float dist_diff = fabsf(current_dist - pre_fall_dist);

			// When the subject was already resting before the impact (e.g. in bed),
			// require a larger displacement to avoid triggering on a bed roll or
			// posture shift.
			float required_dist =
			    was_resting_before_impact
			    ? global_config.fall_dist_threshold * 2.0f
			    : global_config.fall_dist_threshold;

			if (dist_diff >= required_dist)
			{
				// Subject is at a position significantly different from before the impact
				fallen_pos_cnt++;
				recovery_cnt = 0;

				uint32_t confirm_frames =
				    (uint32_t)(global_config.confirm_period_sec * SAMPLE_RATE_HZ);

				if (fallen_pos_cnt >= confirm_frames)
				{
					sys_mode = MODE_ALARM;
					printf("\n[FALL] ALARM — subject at %.2f m for %u s\n",
					       current_dist, global_config.confirm_period_sec);
					return FALL_STATUS_ALARM;
				}
			}
			else if (dist_diff < required_dist * 0.5f)
			{
				// Subject has returned close to their original position
				recovery_cnt++;
				fallen_pos_cnt = (fallen_pos_cnt > 2) ? fallen_pos_cnt - 2 : 0;

				if (recovery_cnt >= RECOVERY_MIN_FRAMES)
				{
					printf("[FALL] Cancelled: subject returned to original position\n");
					sys_mode = MODE_NORMAL;
					return FALL_STATUS_NORMAL;
				}
			}
			else
			{
				// Intermediate zone: neither fallen nor recovered — reset recovery streak
				recovery_cnt = 0;
			}

			// --- Motion-based cancel ---
			// Only cancel on very strong sustained motion (subject clearly standing and
			// walking away). Normal struggling on the floor should NOT cancel.
			if (intra_score > CANCEL_SCORE_THR)
			{
				strong_motion_cnt++;

				if (strong_motion_cnt >= CANCEL_MIN_FRAMES)
				{
					printf("[FALL] Cancelled: strong sustained motion detected\n");
					sys_mode = MODE_NORMAL;
					return FALL_STATUS_NORMAL;
				}
			}
			else
			{
				strong_motion_cnt = 0;
			}

			// --- Timeout ---
			if (suspected_total >= SUSPECTED_TIMEOUT)
			{
				printf("[FALL] Cancelled: timeout (no confirmation within 30 s)\n");
				sys_mode = MODE_NORMAL;
				return FALL_STATUS_NORMAL;
			}

			return FALL_STATUS_SUSPECTED;
		}

		case MODE_ALARM:
		{
			if (alarm_print_counter++ >= (uint32_t)(2.0f * SAMPLE_RATE_HZ))
			{
				printf("\n[CRITICAL] Fall detected!\n");
				alarm_print_counter = 0;
			}

			return FALL_STATUS_ALARM;
		}
	}

	return FALL_STATUS_NORMAL;
}
