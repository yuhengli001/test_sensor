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


static bool do_sensor_calibration(acc_sensor_t     *sensor,
                                  acc_cal_result_t *cal_result,
                                  void             *buffer,
                                  uint32_t          buffer_size);

static void cleanup(acc_detector_presence_handle_t *presence_handle,
                    acc_detector_presence_config_t *presence_config,
                    acc_sensor_t                   *sensor,
                    void                           *buffer);


// Phase unwrapping state for the locked range bin
static float    prev_angle           = 0.0f;
static float    unwrapped_angle      = 0.0f;
static bool     first_phase          = true;
static uint32_t missing_target_count = 0;
static float    ema_dist             = 0.0f; // Distance EMA low-pass filter output


// Extract the mean phasor from a range bin across all sweeps in one frame,
// then update the running unwrapped phase.
// If out_amp is non-NULL, the mean phasor magnitude (coherence indicator) is written to it.
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


int acc_example_detector_distance(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	acc_detector_presence_config_t  *presence_config = NULL;
	acc_detector_presence_handle_t  *presence_handle = NULL;
	acc_detector_presence_metadata_t metadata;
	acc_sensor_t                    *sensor      = NULL;
	void                            *buffer      = NULL;
	uint32_t                         buffer_size = 0U;

	printf("Acconeer software version %s\n", acc_version_get());

	const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();

	if (!acc_rss_hal_register(hal))
	{
		return EXIT_FAILURE;
	}

	presence_config = acc_detector_presence_config_create();

	if (presence_config == NULL)
	{
		printf("acc_detector_presence_config_create() failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	// Presence detector configuration — high-accuracy close-range phase mode:
	//   Range 0.25–1.2 m, fixed step length, high signal quality (60), 128 sweeps/frame.
	//   Intra detection threshold 1.1 (micro-movements), inter threshold 0.8 (gross movements).
	acc_detector_presence_config_start_set(presence_config, 0.25f);
	acc_detector_presence_config_end_set(presence_config, 1.2f);
	acc_detector_presence_config_automatic_subsweeps_set(presence_config, false);
	acc_detector_presence_config_signal_quality_set(presence_config, 60.0f);
	acc_detector_presence_config_sweeps_per_frame_set(presence_config, 128);
	acc_detector_presence_config_frame_rate_set(presence_config, SAMPLE_RATE_HZ);
	acc_detector_presence_config_intra_detection_set(presence_config, true);
	acc_detector_presence_config_intra_detection_threshold_set(presence_config, 1.1f);
	acc_detector_presence_config_inter_detection_set(presence_config, true);
	acc_detector_presence_config_inter_detection_threshold_set(presence_config, 0.8f);

	presence_handle = acc_detector_presence_create(presence_config, &metadata);

	if (presence_handle == NULL)
	{
		printf("acc_detector_presence_create() failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	if (!acc_detector_presence_get_buffer_size(presence_handle, &buffer_size))
	{
		printf("acc_detector_presence_get_buffer_size() failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	buffer = acc_integration_mem_alloc(buffer_size);

	if (buffer == NULL)
	{
		printf("buffer allocation failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	acc_hal_integration_sensor_supply_on(SENSOR_ID);
	acc_hal_integration_sensor_enable(SENSOR_ID);

	sensor = acc_sensor_create(SENSOR_ID);

	if (sensor == NULL)
	{
		printf("acc_sensor_create() failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	acc_cal_result_t cal_result;

	if (!do_sensor_calibration(sensor, &cal_result, buffer, buffer_size))
	{
		printf("do_sensor_calibration() failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	if (!acc_detector_presence_prepare(presence_handle, presence_config, sensor, &cal_result,
	                                   buffer, buffer_size))
	{
		printf("acc_detector_presence_prepare() failed\n");
		cleanup(presence_handle, presence_config, sensor, buffer);
		return EXIT_FAILURE;
	}

	// Initialise sub-modules
	fall_detector_init();
	vital_signs_init();
	first_phase     = true;
	unwrapped_angle = 0.0f;

	printf("Entering main loop with presence detector (phase mode)...\n");

	// Three-phase state machine: searching → coarse sweep → fine measurement
	typedef enum
	{
		DET_SEARCHING,
		DET_COARSE,
		DET_MEASURING,
	} det_phase_t;

	det_phase_t det_phase          = DET_SEARCHING;
	int         tracked_index      = -1;   // Currently locked range bin (-1 = unlocked)
	uint32_t    phase_renorm_counter = 0;  // Periodic phase reference reset (every 10 min)

	// Coarse sweep: track phase state for all candidate bins in parallel
	int   candidate_bins[COARSE_MAX_CANDS];
	int   n_candidates                        = 0;
	float cand_prev_angle[COARSE_MAX_CANDS];
	float cand_unwrapped[COARSE_MAX_CANDS];
	bool  cand_first_phase[COARSE_MAX_CANDS];

	while (true)
	{
		acc_detector_presence_result_t result;

		if (!acc_sensor_measure(sensor))
		{
			printf("acc_sensor_measure failed\n");
			cleanup(presence_handle, presence_config, sensor, buffer);
			return EXIT_FAILURE;
		}

		if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS))
		{
			printf("Sensor interrupt timeout\n");
			cleanup(presence_handle, presence_config, sensor, buffer);
			return EXIT_FAILURE;
		}

		if (!acc_sensor_read(sensor, buffer, buffer_size))
		{
			printf("acc_sensor_read failed\n");
			cleanup(presence_handle, presence_config, sensor, buffer);
			return EXIT_FAILURE;
		}

		if (!acc_detector_presence_process(presence_handle, buffer, &result))
		{
			printf("acc_detector_presence_process failed\n");
			cleanup(presence_handle, presence_config, sensor, buffer);
			return EXIT_FAILURE;
		}

		// Handle periodic recalibration requests from the detector
		if (result.processing_result.calibration_needed)
		{
			printf("Sensor recalibration needed...\n");

			if (!do_sensor_calibration(sensor, &cal_result, buffer, buffer_size))
			{
				printf("do_sensor_calibration() failed\n");
				cleanup(presence_handle, presence_config, sensor, buffer);
				return EXIT_FAILURE;
			}

			if (!acc_detector_presence_prepare(presence_handle, presence_config, sensor,
			                                   &cal_result, buffer, buffer_size))
			{
				printf("acc_detector_presence_prepare() failed\n");
				cleanup(presence_handle, presence_config, sensor, buffer);
				return EXIT_FAILURE;
			}

			continue; // Skip processing for this frame
		}

		// Presence detector range must match the config values above
		const float RANGE_START = 0.25f;
		const float RANGE_END   = 1.2f;

		if (result.depthwise_presence_scores_length < 2)
		{
			continue;
		}

		float step_length = (RANGE_END - RANGE_START) / (result.depthwise_presence_scores_length - 1);

		// --- Smart target lock: presence / absence handling ---
		if (result.presence_detected)
		{
			missing_target_count = 0;

			if (det_phase == DET_SEARCHING)
			{
				// Start a full-range coarse sweep to identify the best range bin
				det_phase    = DET_COARSE;
				n_candidates = 0;

				for (uint32_t b = 0; b < result.depthwise_presence_scores_length; b++)
				{
					if (n_candidates < COARSE_MAX_CANDS)
					{
						candidate_bins[n_candidates]   = b;
						cand_prev_angle[n_candidates]  = 0.0f;
						cand_unwrapped[n_candidates]   = 0.0f;
						cand_first_phase[n_candidates] = true;
						n_candidates++;
					}
				}

				vital_signs_coarse_start();
				printf("[System] Target appeared, starting full frequency scan (%d bins)...\n",
				       n_candidates);
			}
		}
		else
		{
			if (det_phase == DET_MEASURING && tracked_index != -1)
			{
				missing_target_count++;

				// Allow up to 10 s of "stillness" before dropping the target lock
				if (missing_target_count > (uint32_t)(10.0f * SAMPLE_RATE_HZ))
				{
					tracked_index        = -1;
					ema_dist             = 0.0f;
					missing_target_count = 0;
					det_phase            = DET_SEARCHING;
					printf("[System] Target lost (timeout), returning to search...\n");
				}
			}
			else if (det_phase == DET_SEARCHING)
			{
				tracked_index = -1;
				ema_dist      = 0.0f;
			}
			// DET_COARSE: let the coarse sweep complete; FFT results are simply invalid without signal
		}

		uint16_t spf = acc_detector_presence_config_sweeps_per_frame_get(presence_config);

		// --- Main state machine ---
		if (det_phase == DET_COARSE)
		{
			// Update phase for all candidate bins and feed into the coarse sweep buffer
			for (int c = 0; c < n_candidates; c++)
			{
				float ua = update_phase(&result, &metadata, spf, candidate_bins[c],
				                        &cand_prev_angle[c], &cand_first_phase[c],
				                        &cand_unwrapped[c], NULL);
				vital_signs_coarse_feed(c, ua);
			}

			if (vital_signs_coarse_tick())
			{
				int best = vital_signs_coarse_pick_best(n_candidates);

				if (best >= 0)
				{
					tracked_index = candidate_bins[best];

					// Hot-start the fine filter by replaying coarse data; reduces FFT wait time
					vital_signs_init();
					vital_signs_replay_coarse(best);

					// Inherit coarse phase state to maintain continuity
					prev_angle      = cand_prev_angle[best];
					unwrapped_angle = cand_unwrapped[best];
					first_phase     = false;

					phase_renorm_counter = 0;
					det_phase            = DET_MEASURING;

					float raw_dist = RANGE_START + tracked_index * step_length;
					ema_dist       = raw_dist;

					printf("[System] Coarse sweep done; best dist %.2f m, switching to fine mode\n",
					       raw_dist);
				}
				else
				{
					printf("[System] Coarse sweep failed to find vital signs, searching again...\n");
					tracked_index = -1;
					ema_dist      = 0.0f;
					det_phase     = DET_SEARCHING;
				}
			}
		}
		else if (det_phase == DET_MEASURING && tracked_index != -1)
		{
			// --- Sub-bin distance tracking using energy centroid ---
			// Interpolate between adjacent bins to get a smooth distance estimate
			float s_lo = (tracked_index > 0)
			    ? result.depthwise_inter_presence_scores[tracked_index - 1] : 0.0f;
			float s_ce = result.depthwise_inter_presence_scores[tracked_index];
			float s_hi = (tracked_index < (int)result.depthwise_presence_scores_length - 1)
			    ? result.depthwise_inter_presence_scores[tracked_index + 1] : 0.0f;

			float sum_s = s_lo + s_ce + s_hi + 0.001f;
			float offset = (s_hi - s_lo) / sum_s; // Range approximately -1.0 to +1.0

			// Hard bin switch when the centroid drifts significantly
			if (offset > 0.4f && s_hi > 2.0f)
			{
				tracked_index++;
				first_phase = true;
				offset      = 0.0f;
			}
			else if (offset < -0.4f && s_lo > 2.0f)
			{
				tracked_index--;
				first_phase = true;
				offset      = 0.0f;
			}
			else
			{
				if (offset >  0.5f) offset =  0.5f;
				if (offset < -0.5f) offset = -0.5f;
			}

			float raw_dist = RANGE_START + (tracked_index + offset) * step_length;

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
			if (++phase_renorm_counter >= (uint32_t)(600.0f * SAMPLE_RATE_HZ))
			{
				first_phase          = true;
				phase_renorm_counter = 0;
				vital_signs_init();
			}

			float coherence = 0.0f;
			update_phase(&result, &metadata, spf, tracked_index,
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
		float fall_dist = (det_phase == DET_MEASURING && tracked_index != -1)
		    ? ema_dist
		    : result.presence_distance;

		fall_status_t fall_status = process_fall_detection(result.intra_presence_score, fall_dist);

		// fall_status values:
		//   FALL_STATUS_NORMAL    (1) - no event
		//   FALL_STATUS_IMPACT    (2) - energy burst accumulating
		//   FALL_STATUS_SUSPECTED (3) - post-impact stillness, confirming
		//   FALL_STATUS_ALARM     (4) - fall confirmed
		(void)fall_status; // Suppress unused-variable warning; use this value for BLE notification
	}

	cleanup(presence_handle, presence_config, sensor, buffer);
	return EXIT_SUCCESS;
}


static void cleanup(acc_detector_presence_handle_t *presence_handle,
                    acc_detector_presence_config_t *presence_config,
                    acc_sensor_t                   *sensor,
                    void                           *buffer)
{
	acc_hal_integration_sensor_disable(SENSOR_ID);
	acc_hal_integration_sensor_supply_off(SENSOR_ID);

	if (presence_config != NULL)
	{
		acc_detector_presence_config_destroy(presence_config);
	}

	if (presence_handle != NULL)
	{
		acc_detector_presence_destroy(presence_handle);
	}

	if (sensor != NULL)
	{
		acc_sensor_destroy(sensor);
	}

	if (buffer != NULL)
	{
		acc_integration_mem_free(buffer);
	}
}


static bool do_sensor_calibration(acc_sensor_t     *sensor,
                                  acc_cal_result_t *cal_result,
                                  void             *buffer,
                                  uint32_t          buffer_size)
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
			status = acc_sensor_calibrate(sensor, &cal_complete, cal_result, buffer, buffer_size);

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
