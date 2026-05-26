#include "radar_sensor.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "app_common.h"
#include "log_module.h"

#include "acc_definitions_a121.h"
#include "acc_detector_distance.h"
#include "acc_hal_definitions_a121.h"
#include "acc_hal_integration_a121.h"
#include "acc_integration.h"
#include "acc_integration_log.h"
#include "acc_rss_a121.h"
#include "acc_sensor.h"
#include "acc_version.h"

typedef enum
{
	// Switch betwwen different preset configurations
	DISTANCE_PRESET_CONFIG_NONE = 0,
	DISTANCE_PRESET_CONFIG_BALANCED,
	DISTANCE_PRESET_CONFIG_HIGH_ACCURACY,
	DISTANCE_PRESET_CONFIG_PERSON_TRACKING,
} distance_preset_config_t;

#define SENSOR_ID (1U)
#define SENSOR_TIMEOUT_MS (2000U)

typedef struct
{
	acc_sensor_t                     *sensor;
	acc_detector_distance_config_t   *config;
	acc_detector_distance_handle_t   *handle;
	void                             *buffer;
	uint32_t                          buffer_size;
	uint8_t                          *detector_cal_result_static;
	uint32_t                          detector_cal_result_static_size;
	acc_detector_cal_result_dynamic_t detector_cal_result_dynamic;
} distance_detector_resources_t;

static distance_detector_resources_t resources = {0};
static acc_cal_result_t sensor_cal_result;
static bool initialized = false;
static bool started = false;
static bool busy = false;

// Tracking Filter configuration and state
static bool     track_locked = false;
static float    locked_distance = 0.0f;
static float    locked_strength = 0.0f;
static uint8_t  lost_frames_counter = 0;

static float    track_min_dist = 0.25f;       // meters
static float    track_max_dist = 1.6f;        // meters
static float    track_strength_thr = 10.0f;    // dB
static float    track_gate = 0.35f;            // meters
static float    track_alpha = 0.25f;          // smoothing factor
static uint8_t  track_max_lost_frames = 5;     // frame count (~0.5s at 10Hz)


static void cleanup(distance_detector_resources_t *resources);
static void set_config(acc_detector_distance_config_t *detector_config, distance_preset_config_t preset);
static bool initialize_detector_resources(distance_detector_resources_t *resources);
static bool do_sensor_calibration(acc_sensor_t *sensor, acc_cal_result_t *sensor_cal_result, void *buffer, uint32_t buffer_size);
static bool do_full_detector_calibration(distance_detector_resources_t *resources, const acc_cal_result_t *sensor_cal_result);
static bool do_detector_calibration_update(distance_detector_resources_t *resources, const acc_cal_result_t *sensor_cal_result);
static bool do_detector_get_next(distance_detector_resources_t  *resources, const acc_cal_result_t *sensor_cal_result, acc_detector_distance_result_t *result);

bool Radar_Sensor_PreInit(void)
{
    if (initialized) return true;

    LOG_INFO_APP("\n");
    LOG_INFO_APP("A121: Acconeer software version %s\n", acc_version_get());

    // Register HAL
    const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();
    if (!acc_rss_hal_register(hal)) {
        return false;
    }

    // Create detector config
    resources.config = acc_detector_distance_config_create();
    if (resources.config == NULL) {
        LOG_INFO_APP("A121: acc_detector_distance_config_create() failed\n");
        return false;
    }

    set_config(resources.config, DISTANCE_PRESET_CONFIG_PERSON_TRACKING);

    // Initial resource allocation
    if (!initialize_detector_resources(&resources)) {
        LOG_INFO_APP("A121: Initializing detector resources failed\n");
        cleanup(&resources);
        return false;
    }

    initialized = true;
    LOG_INFO_APP("A121: Software Pre-Initialization complete\n");
    return true;
}

static bool Radar_Sensor_Reconfigure(void)
{
    bool was_started = started;

    if (was_started) {
        Radar_Sensor_Stop();
    }

    // Free resources that depend on config (buffer size may change)
    if (resources.handle != NULL) {
        acc_detector_distance_destroy(resources.handle);
        resources.handle = NULL;
    }
    if (resources.buffer != NULL) {
        acc_integration_mem_free(resources.buffer);
        resources.buffer = NULL;
    }
    if (resources.detector_cal_result_static != NULL) {
        acc_integration_mem_free(resources.detector_cal_result_static);
        resources.detector_cal_result_static = NULL;
    }

    // Re-create and re-allocate
    if (!initialize_detector_resources(&resources)) {
        LOG_INFO_APP("A121: Re-initialization failed\n");
        return false;
    }

    if (was_started) {
        LOG_INFO_APP("A121: Config Updated. Sensor is now OFF. Send '02 01' to restart.\n");
    }

    return true;
}

bool Radar_Sensor_UpdateParam(radar_param_id_t param_id, void *value)
{
    if (!initialized) return false;

    LOG_INFO_APP("A121: Updating Param ID 0x%02X\n", param_id);

    switch (param_id) {
        case RADAR_PARAM_RANGE_START:
            acc_detector_distance_config_start_set(resources.config, *(float*)value);
            break;
        case RADAR_PARAM_RANGE_END:
            acc_detector_distance_config_end_set(resources.config, *(float*)value);
            break;
        case RADAR_PARAM_SENSITIVITY:
            acc_detector_distance_config_threshold_sensitivity_set(resources.config, *(float*)value);
            break;
        case RADAR_PARAM_MAX_PROFILE:
            acc_detector_distance_config_max_profile_set(resources.config, (acc_config_profile_t)*(uint32_t*)value);
            break;
        case RADAR_PARAM_SIGNAL_QUALITY:
            acc_detector_distance_config_signal_quality_set(resources.config, *(float*)value);
            break;
        case RADAR_PARAM_MAX_STEP_LENGTH:
            acc_detector_distance_config_max_step_length_set(resources.config, (uint16_t)*(uint32_t*)value);
            break;
        case RADAR_PARAM_PEAK_SORTING:
            acc_detector_distance_config_peak_sorting_set(resources.config, (acc_detector_distance_peak_sorting_t)*(uint32_t*)value);
            break;
        case RADAR_PARAM_THRESH_METHOD:
            acc_detector_distance_config_threshold_method_set(resources.config, (acc_detector_distance_threshold_method_t)*(uint32_t*)value);
            break;
        case RADAR_PARAM_REFLECTOR_SHAPE:
            acc_detector_distance_config_reflector_shape_set(resources.config, (acc_detector_distance_reflector_shape_t)*(uint32_t*)value);
            break;
        case RADAR_PARAM_LEAKAGE_CANCEL:
            acc_detector_distance_config_close_range_leakage_cancellation_set(resources.config, *(uint32_t*)value != 0);
            break;
        case RADAR_PARAM_NUM_FRAMES:
            acc_detector_distance_config_num_frames_recorded_threshold_set(resources.config, (uint16_t)*(uint32_t*)value);
            break;
        case RADAR_PARAM_FIXED_AMP_THR:
            acc_detector_distance_config_fixed_amplitude_threshold_value_set(resources.config, *(float*)value);
            break;
        case RADAR_PARAM_FIXED_STR_THR:
            acc_detector_distance_config_fixed_strength_threshold_value_set(resources.config, *(float*)value);
            break;
        case RADAR_PARAM_TRACK_MIN_DIST:
            track_min_dist = *(float*)value;
            LOG_INFO_APP("A121: track_min_dist set to %.3f m\n", track_min_dist);
            return true; // software parameter: no reconfigure needed
        case RADAR_PARAM_TRACK_MAX_DIST:
            track_max_dist = *(float*)value;
            LOG_INFO_APP("A121: track_max_dist set to %.3f m\n", track_max_dist);
            return true;
        case RADAR_PARAM_TRACK_STRENGTH:
            track_strength_thr = *(float*)value;
            LOG_INFO_APP("A121: track_strength_thr set to %.1f dB\n", track_strength_thr);
            return true;
        case RADAR_PARAM_TRACK_GATE:
            track_gate = *(float*)value;
            LOG_INFO_APP("A121: track_gate set to %.3f m\n", track_gate);
            return true;
        default:
            return false;
    }

    return Radar_Sensor_Reconfigure();
}

bool Radar_Sensor_Start(void)
{
    if (!initialized) return false;
    if (started) return true;
    if (busy) return false; // Don't start if another operation is in progress

    busy = true;
    LOG_INFO_APP("A121: Starting Sensor Calibration...\n");

    acc_hal_integration_sensor_supply_on(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    resources.sensor = acc_sensor_create(SENSOR_ID);
    if (resources.sensor == NULL) {
        Radar_Sensor_Stop();
        return false;
    }

    if (!do_sensor_calibration(resources.sensor, &sensor_cal_result, resources.buffer, resources.buffer_size)) {
        Radar_Sensor_Stop();
        return false;
    }

    if (!do_full_detector_calibration(&resources, &sensor_cal_result)) {
        Radar_Sensor_Stop();
        return false;
    }

    started = true;
    busy = false;
    LOG_INFO_APP("A121: Sensor started with new config!\n");
    return true;
}

bool Radar_Sensor_Get_Next_Results(float *distances_m, float *strengths_db, uint8_t *num_targets)
{
    if (!initialized || !started) return false;
    if (busy) return false; // Skip if sensor is busy with a previous request

    busy = true;

    acc_detector_distance_result_t result = {0};

    if (!do_detector_get_next(&resources, &sensor_cal_result, &result)) {
        busy = false;
        return false;
    }

    if (result.calibration_needed) {
        if (!do_sensor_calibration(resources.sensor, &sensor_cal_result, resources.buffer, resources.buffer_size)) {
            busy = false;
            return false;
        }
        if (!do_detector_calibration_update(&resources, &sensor_cal_result)) {
            busy = false;
            return false;
        }
    }

    // --- Closest-Target Tracking Filter State Machine ---
    
    // Find the closest valid candidate in the current frame
    float closest_cand_dist = 999.0f;
    float closest_cand_str = -999.0f;
    bool found_candidate = false;

    for (uint8_t i = 0; i < result.num_distances; i++) {
        float d = result.distances[i];
        float s = result.strengths[i];
        
        // Target must be within the presentation zone and satisfy strength threshold
        if (d >= track_min_dist && d <= track_max_dist && s >= track_strength_thr) {
            if (d < closest_cand_dist) {
                closest_cand_dist = d;
                closest_cand_str = s;
                found_candidate = true;
            }
        }
    }

    if (track_locked) {
        // We are currently locked on a target
        bool match_found = false;
        float best_match_dist = 999.0f;
        float best_match_str = -999.0f;

        // 1. Search for a target closest to the tracked distance within the gate
        for (uint8_t i = 0; i < result.num_distances; i++) {
            float d = result.distances[i];
            float s = result.strengths[i];

            if (s >= track_strength_thr && d >= track_min_dist && d <= track_max_dist) {
                float diff = fabsf(d - locked_distance);

                if (diff <= track_gate) {
                    if (diff < fabsf(best_match_dist - locked_distance)) {
                        best_match_dist = d;
                        best_match_str = s;
                        match_found = true;
                    }
                }
            }
        }

        // 2. Also check if a new valid candidate has appeared that is CLOSER than our tracked target.
        // If a new candidate is closer, we immediately switch lock to it (presenter walked closer).
        if (found_candidate && closest_cand_dist < (locked_distance - 0.15f)) {
            locked_distance = closest_cand_dist;
            locked_strength = closest_cand_str;
            lost_frames_counter = 0;
            LOG_INFO_APP("Radar Track: Switched to closer target at %.3f m\n", locked_distance);
        }
        else if (match_found) {
            // Update locked track using Alpha-Beta smoothing
            locked_distance = (track_alpha * best_match_dist) + ((1.0f - track_alpha) * locked_distance);
            locked_strength = (track_alpha * best_match_str) + ((1.0f - track_alpha) * locked_strength);
            lost_frames_counter = 0;
        } else {
            // No matching target in the gate. Enter Coasting mode.
            lost_frames_counter++;
            if (lost_frames_counter >= track_max_lost_frames) {
                track_locked = false;
                LOG_INFO_APP("Radar Track: Lock Lost\n");
            }
        }
    } else {
        // Search mode: Look for the closest valid target to initiate lock
        if (found_candidate) {
            track_locked = true;
            locked_distance = closest_cand_dist;
            locked_strength = closest_cand_str;
            lost_frames_counter = 0;
            LOG_INFO_APP("Radar Track: Target Locked at %.3f m (Str: %.1f dB)\n", locked_distance, locked_strength);
        }
    }

    // --- Format Output ---
    if (track_locked) {
        distances_m[0] = locked_distance;
        strengths_db[0] = locked_strength;
        *num_targets = 1;
    } else {
        *num_targets = 0;
    }

    busy = false;
    return true;
}

bool Radar_Sensor_Get_Tracked_State(float *distance_m, float *strength_db)
{
    if (!initialized || !started) return false;
    if (distance_m != NULL) *distance_m = locked_distance;
    if (strength_db != NULL) *strength_db = locked_strength;
    return track_locked;
}

void Radar_Sensor_Stop(void)
{
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_supply_off(SENSOR_ID);

    if (resources.sensor != NULL) {
        acc_sensor_destroy(resources.sensor);
        resources.sensor = NULL;
    }

    started = false;
    busy = false;
}

void Radar_Sensor_Cleanup(void)
{
    if (!initialized) return;
    Radar_Sensor_Stop();
    cleanup(&resources);
    initialized = false;
}

/* -------------------------------------------------------------------------- */
/* HELPER FUNCTIONS (Migrated directly from example_detector_distance.c)      */
/* -------------------------------------------------------------------------- */

static void cleanup(distance_detector_resources_t *res)
{
    if (res->config != NULL) {
        acc_detector_distance_config_destroy(res->config);
        res->config = NULL;
    }
    if (res->handle != NULL) {
        acc_detector_distance_destroy(res->handle);
        res->handle = NULL;
    }
    acc_integration_mem_free(res->buffer);
    res->buffer = NULL;
    acc_integration_mem_free(res->detector_cal_result_static);
    res->detector_cal_result_static = NULL;
}

static void set_config(acc_detector_distance_config_t *detector_config, distance_preset_config_t preset)
{
	switch (preset)
	{
		case DISTANCE_PRESET_CONFIG_NONE:
			break;
		case DISTANCE_PRESET_CONFIG_BALANCED:
			acc_detector_distance_config_start_set(detector_config, 0.25f);
			acc_detector_distance_config_end_set(detector_config, 3.0f);
			acc_detector_distance_config_max_step_length_set(detector_config, 0U);
			acc_detector_distance_config_max_profile_set(detector_config, ACC_CONFIG_PROFILE_5);
			acc_detector_distance_config_reflector_shape_set(detector_config, ACC_DETECTOR_DISTANCE_REFLECTOR_SHAPE_GENERIC);
			acc_detector_distance_config_peak_sorting_set(detector_config, ACC_DETECTOR_DISTANCE_PEAK_SORTING_STRONGEST);
			acc_detector_distance_config_threshold_method_set(detector_config, ACC_DETECTOR_DISTANCE_THRESHOLD_METHOD_CFAR);
			acc_detector_distance_config_threshold_sensitivity_set(detector_config, 0.5f);
			acc_detector_distance_config_signal_quality_set(detector_config, 15.0f);
			acc_detector_distance_config_close_range_leakage_cancellation_set(detector_config, false);
			break;
		case DISTANCE_PRESET_CONFIG_HIGH_ACCURACY:
			acc_detector_distance_config_start_set(detector_config, 0.25f);
			acc_detector_distance_config_end_set(detector_config, 3.0f);
			acc_detector_distance_config_max_step_length_set(detector_config, 2U);
			acc_detector_distance_config_max_profile_set(detector_config, ACC_CONFIG_PROFILE_3);
			acc_detector_distance_config_reflector_shape_set(detector_config, ACC_DETECTOR_DISTANCE_REFLECTOR_SHAPE_GENERIC);
			acc_detector_distance_config_peak_sorting_set(detector_config, ACC_DETECTOR_DISTANCE_PEAK_SORTING_STRONGEST);
			acc_detector_distance_config_threshold_method_set(detector_config, ACC_DETECTOR_DISTANCE_THRESHOLD_METHOD_CFAR);
			acc_detector_distance_config_threshold_sensitivity_set(detector_config, 0.5f);
			acc_detector_distance_config_signal_quality_set(detector_config, 20.0f);
			acc_detector_distance_config_close_range_leakage_cancellation_set(detector_config, false);
			break;
		case DISTANCE_PRESET_CONFIG_PERSON_TRACKING:
			// --- Optimized for close-range (0.25m - 2.0m) human body tracking ---

			// 1. Narrow the sweep range: only scan 0.25m - 2.0m
			//    Less range = faster sweep, fewer background noise targets, better SNR per frame
			acc_detector_distance_config_start_set(detector_config, 0.25f);
			acc_detector_distance_config_end_set(detector_config, 2.0f);

			// 2. Profile 3 instead of Profile 5
			//    Profile 5 uses the longest pulse → best for >3m but causes multipath issues at close range
			//    Profile 3 uses a shorter pulse → much better distance resolution and SNR at 0.5-2m
			acc_detector_distance_config_max_profile_set(detector_config, ACC_CONFIG_PROFILE_3);

			// 3. Finer step length (2) for higher distance resolution
			//    Auto (0) may pick large steps that miss subtle distance changes when person sways
			acc_detector_distance_config_max_step_length_set(detector_config, 2U);

			// 4. CLOSEST peak sorting: always return the nearest target first
			//    This directly matches the requirement: "离传感器最近的一定是我要测得目标"
			acc_detector_distance_config_peak_sorting_set(detector_config, ACC_DETECTOR_DISTANCE_PEAK_SORTING_CLOSEST);

			// 5. Generic reflector shape (correct for human body, which is not a flat plate)
			acc_detector_distance_config_reflector_shape_set(detector_config, ACC_DETECTOR_DISTANCE_REFLECTOR_SHAPE_GENERIC);

			// 6. CFAR threshold method: adaptive threshold that works well in dynamic environments
			acc_detector_distance_config_threshold_method_set(detector_config, ACC_DETECTOR_DISTANCE_THRESHOLD_METHOD_CFAR);

			// 7. Lower sensitivity threshold (0.35 vs 0.5): makes it EASIER to detect weak human reflections
			//    Human body is a weak reflector compared to walls/metal; 0.5 may cause missed detections
			acc_detector_distance_config_threshold_sensitivity_set(detector_config, 0.35f);

			// 8. Higher signal quality (25 vs 15): more hardware averaging per frame
			//    Trade-off: slightly slower (~10ms more per frame), but MUCH more stable readings
			//    Critical for demo: eliminates the "jumping readings" problem
			acc_detector_distance_config_signal_quality_set(detector_config, 25.0f);

			// 9. Enable close-range leakage cancellation
			//    At 0.25-2m the sensor's own TX-RX coupling creates ghost targets
			//    This filter removes them — essential for reliable close-range detection
			acc_detector_distance_config_close_range_leakage_cancellation_set(detector_config, true);
			break;
	}
}

static bool initialize_detector_resources(distance_detector_resources_t *res)
{
	res->handle = acc_detector_distance_create(res->config);
	if (res->handle == NULL) return false;

	if (!acc_detector_distance_get_sizes(res->handle, &(res->buffer_size), &(res->detector_cal_result_static_size))) return false;

	res->buffer = acc_integration_mem_alloc(res->buffer_size);
	if (res->buffer == NULL) return false;

	res->detector_cal_result_static = acc_integration_mem_alloc(res->detector_cal_result_static_size);
	if (res->detector_cal_result_static == NULL) return false;

	return true;
}

static bool do_sensor_calibration(acc_sensor_t *sensor, acc_cal_result_t *sensor_cal_res, void *buffer, uint32_t buffer_size)
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
			status = acc_sensor_calibrate(sensor, &cal_complete, sensor_cal_res, buffer, buffer_size);

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

static bool do_full_detector_calibration(distance_detector_resources_t *res, const acc_cal_result_t *sensor_cal_res)
{
	bool done = false;
	bool status;
	do
	{
		status = acc_detector_distance_calibrate(res->sensor, res->handle, sensor_cal_res, res->buffer, res->buffer_size,
		                                         res->detector_cal_result_static, res->detector_cal_result_static_size,
		                                         &res->detector_cal_result_dynamic, &done);

		if (status && !done)
		{
			status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
		}
	} while (status && !done);
	return status;
}

static bool do_detector_calibration_update(distance_detector_resources_t *res, const acc_cal_result_t *sensor_cal_res)
{
	bool done = false;
	bool status;
	do
	{
		status = acc_detector_distance_update_calibration(res->sensor, res->handle, sensor_cal_res, res->buffer, res->buffer_size,
		                                                  &res->detector_cal_result_dynamic, &done);
		if (status && !done)
		{
			status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
		}
	} while (status && !done);
	return status;
}

static bool do_detector_get_next(distance_detector_resources_t *res, const acc_cal_result_t *sensor_cal_res, acc_detector_distance_result_t *result)
{
	bool result_available = false;
	do
	{
		// Prepare sensor
		if (!acc_detector_distance_prepare(res->handle, res->config, res->sensor, sensor_cal_res, res->buffer, res->buffer_size)) return false;
		// Measure
		if (!acc_sensor_measure(res->sensor)) return false;
		// Wait for sensor interrupt
		if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS)) return false;
		// Read sensor data
		if (!acc_sensor_read(res->sensor, res->buffer, res->buffer_size)) return false;

		// Process sensor data	
		if (!acc_detector_distance_process(res->handle, res->buffer, res->detector_cal_result_static,
		                                   &res->detector_cal_result_dynamic, &result_available, result))
		{
			return false;
		}
	} while (!result_available);

	return true;
}
