// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc_definitions_a121.h"
#include "acc_detector_presence.h"
#include "acc_hal_definitions_a121.h"
#include "acc_hal_integration_a121.h"
#include "acc_integration.h"
#include "acc_integration_log.h"
#include "acc_rss_a121.h"
#include "acc_sensor.h"
#include "acc_version.h"

#include "app_config.h"
#include "fall_detector.h"
#include "vital_signs.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define SENSOR_ID         (1U)
#define SENSOR_TIMEOUT_MS (2000U)

// Presence detector range — must match acc_detector_presence_config_{start,end}_set() below
#define RANGE_START 0.25f
#define RANGE_END   1.2f


// -------------------------------------------------------------------------
// Phase tracking state for the locked range bin
// -------------------------------------------------------------------------
static float    prev_angle           = 0.0f;
static float    unwrapped_angle      = 0.0f;
static bool     first_phase          = true;
static uint32_t missing_target_count = 0;
static float    ema_dist             = 0.0f;


// -------------------------------------------------------------------------
// Sensor / detector handles — persist across step() calls
// -------------------------------------------------------------------------
static acc_detector_presence_config_t  *s_config      = NULL;
static acc_detector_presence_handle_t  *s_handle      = NULL;
static acc_detector_presence_metadata_t s_meta;
static acc_sensor_t                    *s_sensor      = NULL;
static void                            *s_buffer      = NULL;
static uint32_t                         s_buffer_size = 0U;
static acc_cal_result_t                 s_cal_result;


// -------------------------------------------------------------------------
// Main state-machine state — persist across step() calls
// -------------------------------------------------------------------------
typedef enum
{
	DET_SEARCHING,  // No target; waiting for presence
	DET_COARSE,     // Collecting phase data across all bins to pick the best one
	DET_MEASURING,  // Locked onto a bin; running vital signs + fall detection
} det_phase_t;

static det_phase_t  s_det_phase        = DET_SEARCHING;
static int          s_tracked_index    = -1;   // Locked range bin (-1 = unlocked)
static uint32_t     s_phase_renorm_cnt = 0;    // Counts frames since last phase reset

// Coarse sweep: parallel phase tracking across all candidate bins
static int   s_candidate_bins[COARSE_MAX_CANDS];
static int   s_n_candidates = 0;
static float s_cand_prev_angle[COARSE_MAX_CANDS];
static float s_cand_unwrapped[COARSE_MAX_CANDS];
static bool  s_cand_first_phase[COARSE_MAX_CANDS];


// -------------------------------------------------------------------------
// Internal helpers
// -------------------------------------------------------------------------

// Extract the mean phasor across all sweeps in a frame, then update running
// unwrapped phase for the given bin.  If out_amp is non-NULL, phasor magnitude
// (coherence indicator) is written there.
static float update_phase(const acc_detector_presence_result_t   *res,
                           const acc_detector_presence_metadata_t *meta,
                           uint16_t                                spf,
                           int                                     bin,
                           float                                  *prev_a,
                           bool                                   *first,
                           float                                  *unwrapped,
                           float                                  *out_amp)
{
	float complex mean = 0.0f + 0.0f * I;

	for (int s = 0; s < spf; s++)
	{
		acc_int16_complex_t p = res->processing_result.frame[s * meta->num_points + bin];
		mean += (float)p.real + (float)p.imag * I;
	}

	mean /= spf;

	if (out_amp != NULL)
	{
		*out_amp = cabsf(mean);
	}

	float angle = cargf(mean);

	if (*first)
	{
		*prev_a    = angle;
		*unwrapped = angle;
		*first     = false;
	}
	else
	{
		float diff = angle - *prev_a;

		if (diff >  M_PI) diff -= 2.0f * M_PI;
		if (diff < -M_PI) diff += 2.0f * M_PI;

		*unwrapped += diff;
		*prev_a     = angle;
	}

	return *unwrapped;
}


static void cleanup_resources(void)
{
	acc_hal_integration_sensor_disable(SENSOR_ID);
	acc_hal_integration_sensor_supply_off(SENSOR_ID);

	if (s_config != NULL)
	{
		acc_detector_presence_config_destroy(s_config);
		s_config = NULL;
	}

	if (s_handle != NULL)
	{
		acc_detector_presence_destroy(s_handle);
		s_handle = NULL;
	}

	if (s_sensor != NULL)
	{
		acc_sensor_destroy(s_sensor);
		s_sensor = NULL;
	}

	if (s_buffer != NULL)
	{
		acc_integration_mem_free(s_buffer);
		s_buffer = NULL;
	}

	// Reset shared BLE output state so the notification layer stops sending stale data
	g_presence_valid = false;
	g_presence_dist  = 0.0f;
	g_fall_status    = FALL_STATUS_NORMAL;
}


// Sensor calibration with one automatic retry on failure.
static bool do_sensor_calibration(void)
{
	bool           status              = false;
	bool           cal_complete        = false;
	const uint16_t calibration_retries = 1U;

	for (uint16_t i = 0; !status && (i <= calibration_retries); i++)
	{
		acc_hal_integration_sensor_disable(SENSOR_ID);
		acc_hal_integration_sensor_enable(SENSOR_ID);

		do
		{
			status = acc_sensor_calibrate(s_sensor, &cal_complete, &s_cal_result,
			                              s_buffer, s_buffer_size);

			if (status && !cal_complete)
			{
				status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
			}
		} while (status && !cal_complete);
	}

	if (status)
	{
		acc_hal_integration_sensor_disable(SENSOR_ID);
		acc_hal_integration_sensor_enable(SENSOR_ID);
	}

	return status;
}


// -------------------------------------------------------------------------
// Public API — split init / step / deinit
// -------------------------------------------------------------------------

int acc_example_detector_distance_init(void)
{
	printf("Acconeer software version %s\n", acc_version_get());

	const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();

	if (!acc_rss_hal_register(hal))
	{
		return EXIT_FAILURE;
	}

	s_config = acc_detector_presence_config_create();

	if (s_config == NULL)
	{
		printf("acc_detector_presence_config_create() failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	// Presence detector configuration — high-accuracy close-range phase mode:
	//   Range 0.25–1.2 m, fixed step length, high signal quality (60), 128 sweeps/frame.
	//   Intra detection threshold 1.1 (micro-movements), inter threshold 0.8 (gross movements).
	acc_detector_presence_config_start_set(s_config, RANGE_START);
	acc_detector_presence_config_end_set(s_config, RANGE_END);
	acc_detector_presence_config_automatic_subsweeps_set(s_config, false);
	acc_detector_presence_config_signal_quality_set(s_config, 60.0f);
	acc_detector_presence_config_sweeps_per_frame_set(s_config, 128);
	acc_detector_presence_config_frame_rate_set(s_config, SAMPLE_RATE_HZ);
	acc_detector_presence_config_intra_detection_set(s_config, true);
	acc_detector_presence_config_intra_detection_threshold_set(s_config, 1.1f);
	acc_detector_presence_config_inter_detection_set(s_config, true);
	acc_detector_presence_config_inter_detection_threshold_set(s_config, 0.8f);

	s_handle = acc_detector_presence_create(s_config, &s_meta);

	if (s_handle == NULL)
	{
		printf("acc_detector_presence_create() failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	if (!acc_detector_presence_get_buffer_size(s_handle, &s_buffer_size))
	{
		printf("acc_detector_presence_get_buffer_size() failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	s_buffer = acc_integration_mem_alloc(s_buffer_size);

	if (s_buffer == NULL)
	{
		printf("buffer allocation failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	acc_hal_integration_sensor_supply_on(SENSOR_ID);
	acc_hal_integration_sensor_enable(SENSOR_ID);

	s_sensor = acc_sensor_create(SENSOR_ID);

	if (s_sensor == NULL)
	{
		printf("acc_sensor_create() failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	if (!do_sensor_calibration())
	{
		printf("do_sensor_calibration() failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	if (!acc_detector_presence_prepare(s_handle, s_config, s_sensor, &s_cal_result,
	                                   s_buffer, s_buffer_size))
	{
		printf("acc_detector_presence_prepare() failed\n");
		cleanup_resources();
		return EXIT_FAILURE;
	}

	// Initialise sub-modules
	fall_detector_init();
	vital_signs_init();

	// Reset all persisted state
	first_phase          = true;
	unwrapped_angle      = 0.0f;
	ema_dist             = 0.0f;
	missing_target_count = 0;
	s_det_phase          = DET_SEARCHING;
	s_tracked_index      = -1;
	s_phase_renorm_cnt   = 0;
	s_n_candidates       = 0;
	g_presence_valid     = false;
	g_presence_dist      = 0.0f;
	g_fall_status        = FALL_STATUS_NORMAL;

	printf("Presence detector ready (phase mode, fall detection active).\n");

	return EXIT_SUCCESS;
}


bool acc_example_detector_distance_step(void)
{
	if (s_sensor == NULL)
	{
		// Init was never called or a previous step already cleaned up on error
		return false;
	}

	acc_detector_presence_result_t result;

	if (!acc_sensor_measure(s_sensor))
	{
		printf("acc_sensor_measure failed\n");
		cleanup_resources();
		return false;
	}

	if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS))
	{
		printf("Sensor interrupt timeout\n");
		cleanup_resources();
		return false;
	}

	if (!acc_sensor_read(s_sensor, s_buffer, s_buffer_size))
	{
		printf("acc_sensor_read failed\n");
		cleanup_resources();
		return false;
	}

	if (!acc_detector_presence_process(s_handle, s_buffer, &result))
	{
		printf("acc_detector_presence_process failed\n");
		cleanup_resources();
		return false;
	}

	// Handle periodic recalibration requests from the detector
	if (result.processing_result.calibration_needed)
	{
		printf("Sensor recalibration needed...\n");

		if (!do_sensor_calibration())
		{
			printf("do_sensor_calibration() failed\n");
			cleanup_resources();
			return false;
		}

		if (!acc_detector_presence_prepare(s_handle, s_config, s_sensor,
		                                   &s_cal_result, s_buffer, s_buffer_size))
		{
			printf("acc_detector_presence_prepare() failed\n");
			cleanup_resources();
			return false;
		}

		return true; // Skip processing for this frame
	}

	if (result.depthwise_presence_scores_length < 2)
	{
		return true;
	}

	float step_length = (RANGE_END - RANGE_START) / (result.depthwise_presence_scores_length - 1);

	// --- Smart target lock: presence / absence handling ---
	if (result.presence_detected)
	{
		missing_target_count = 0;

		if (s_det_phase == DET_SEARCHING)
		{
			// Start a full-range coarse sweep to identify the best range bin
			s_det_phase    = DET_COARSE;
			s_n_candidates = 0;

			for (uint32_t b = 0; b < result.depthwise_presence_scores_length; b++)
			{
				if (s_n_candidates < COARSE_MAX_CANDS)
				{
					s_candidate_bins[s_n_candidates]   = (int)b;
					s_cand_prev_angle[s_n_candidates]  = 0.0f;
					s_cand_unwrapped[s_n_candidates]   = 0.0f;
					s_cand_first_phase[s_n_candidates] = true;
					s_n_candidates++;
				}
			}

			vital_signs_coarse_start();
			printf("[System] Target appeared, starting full frequency scan (%d bins)...\n",
			       s_n_candidates);
		}
	}
	else
	{
		if (s_det_phase == DET_MEASURING && s_tracked_index != -1)
		{
			missing_target_count++;

			// Allow up to 10 s of absence before dropping the target lock
			if (missing_target_count > (uint32_t)(10.0f * SAMPLE_RATE_HZ))
			{
				s_tracked_index      = -1;
				ema_dist             = 0.0f;
				missing_target_count = 0;
				s_det_phase          = DET_SEARCHING;
				printf("[System] Target lost (timeout), returning to search...\n");
			}
		}
		else if (s_det_phase == DET_SEARCHING)
		{
			s_tracked_index = -1;
			ema_dist        = 0.0f;
		}
		// DET_COARSE: let the sweep complete; FFT results are simply invalid without signal
	}

	uint16_t spf = acc_detector_presence_config_sweeps_per_frame_get(s_config);

	// --- Main state machine ---
	if (s_det_phase == DET_COARSE)
	{
		// Update phase for all candidate bins and feed into the coarse sweep buffer
		for (int c = 0; c < s_n_candidates; c++)
		{
			float ua = update_phase(&result, &s_meta, spf, s_candidate_bins[c],
			                        &s_cand_prev_angle[c], &s_cand_first_phase[c],
			                        &s_cand_unwrapped[c], NULL);
			vital_signs_coarse_feed(c, ua);
		}

		if (vital_signs_coarse_tick())
		{
			int best = vital_signs_coarse_pick_best(s_n_candidates);

			if (best >= 0)
			{
				s_tracked_index = s_candidate_bins[best];

				// Hot-start the fine filter by replaying coarse data; reduces FFT wait time
				vital_signs_init();
				vital_signs_replay_coarse(best);

				// Inherit coarse phase state to maintain continuity
				prev_angle      = s_cand_prev_angle[best];
				unwrapped_angle = s_cand_unwrapped[best];
				first_phase     = false;

				s_phase_renorm_cnt = 0;
				s_det_phase        = DET_MEASURING;

				float raw_dist = RANGE_START + s_tracked_index * step_length;
				ema_dist       = raw_dist;

				printf("[System] Coarse sweep done; best dist %.2f m, switching to fine mode\n",
				       raw_dist);
			}
			else
			{
				printf("[System] Coarse sweep failed to find vital signs, searching again...\n");
				s_tracked_index = -1;
				ema_dist        = 0.0f;
				s_det_phase     = DET_SEARCHING;
			}
		}
	}
	else if (s_det_phase == DET_MEASURING && s_tracked_index != -1)
	{
		// --- Sub-bin distance tracking using energy centroid ---
		// Interpolate between adjacent bins to get a smooth distance estimate
		float score_lo = (s_tracked_index > 0)
		    ? result.depthwise_inter_presence_scores[s_tracked_index - 1] : 0.0f;
		float score_ce = result.depthwise_inter_presence_scores[s_tracked_index];
		float score_hi = (s_tracked_index < (int)result.depthwise_presence_scores_length - 1)
		    ? result.depthwise_inter_presence_scores[s_tracked_index + 1] : 0.0f;

		float sum_scores = score_lo + score_ce + score_hi + 0.001f;
		float offset     = (score_hi - score_lo) / sum_scores; // Range approximately -1.0 to +1.0

		// Hard bin switch when the centroid drifts significantly
		if (offset > 0.4f && score_hi > 2.0f)
		{
			s_tracked_index++;
			first_phase = true;
			offset      = 0.0f;
		}
		else if (offset < -0.4f && score_lo > 2.0f)
		{
			s_tracked_index--;
			first_phase = true;
			offset      = 0.0f;
		}
		else
		{
			if (offset >  0.5f) offset =  0.5f;
			if (offset < -0.5f) offset = -0.5f;
		}

		float raw_dist = RANGE_START + (s_tracked_index + offset) * step_length;

		if (ema_dist == 0.0f)
		{
			ema_dist = raw_dist;
		}
		else
		{
			ema_dist = 0.05f * raw_dist + 0.95f * ema_dist; // Strong low-pass smoothing
		}

		float current_dist = ema_dist;

		// Periodic phase reference reset to prevent float precision degradation (~10 min)
		if (++s_phase_renorm_cnt >= (uint32_t)(600.0f * SAMPLE_RATE_HZ))
		{
			first_phase        = true;
			s_phase_renorm_cnt = 0;
			vital_signs_init();
		}

		float coherence = 0.0f;
		update_phase(&result, &s_meta, spf, s_tracked_index,
		             &prev_angle, &first_phase, &unwrapped_angle, &coherence);

		printf("[LOCK] Dist: %.3f m | Intra: %.1f | Angle: %.2f | Coh: %.0f\n",
		       current_dist, result.intra_presence_score, unwrapped_angle, coherence);

		process_vital_signs(unwrapped_angle, current_dist);
	}
	else
	{
		if (acc_integration_get_time() % 2000 < 50)
		{
			printf("[System] Searching for target...\n");
		}
	}

	// --- Fall detection (runs every frame regardless of phase-lock state) ---
	// Use smoothed EMA distance when locked; fall back to the presence detector's
	// own rough estimate during coarse sweep or searching phase.
	float fall_dist = (s_det_phase == DET_MEASURING && s_tracked_index != -1)
	    ? ema_dist
	    : result.presence_distance;

	g_fall_status = process_fall_detection(result.intra_presence_score, fall_dist);

	// Publish best available distance and fall status to the BLE layer.
	// Valid whenever presence is actively detected, or during DET_MEASURING
	// (which tolerates up to 10 s of brief absence before dropping the lock).
	g_presence_dist  = fall_dist;
	g_presence_valid = result.presence_detected || (s_det_phase == DET_MEASURING);

	return true;
}


void acc_example_detector_distance_deinit(void)
{
	cleanup_resources();
}


// Keep the original combined function for builds that do not need BLE co-operation.
int acc_example_detector_distance(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	if (acc_example_detector_distance_init() != EXIT_SUCCESS)
	{
		return EXIT_FAILURE;
	}

	while (acc_example_detector_distance_step()) {}

	acc_example_detector_distance_deinit();
	return EXIT_SUCCESS;
}
