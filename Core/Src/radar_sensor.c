#include "radar_sensor.h"

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

    set_config(resources.config, DISTANCE_PRESET_CONFIG_BALANCED);

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
        busy = false;
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
            return false;
        }
        if (!do_detector_calibration_update(&resources, &sensor_cal_result)) {
            return false;
        }
    }

    *num_targets = result.num_distances;
    for (uint8_t i = 0; i < result.num_distances; i++) {
        distances_m[i] = result.distances[i];
        strengths_db[i] = result.strengths[i];
    }

    busy = false;
    return true;
}

void Radar_Sensor_Stop(void)
{
    if (!started) return;

    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_supply_off(SENSOR_ID);

    if (resources.sensor != NULL) {
        acc_sensor_destroy(resources.sensor);
        resources.sensor = NULL;
    }

    started = false;
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
	acc_detector_distance_config_destroy(res->config);
    res->config = NULL;

	acc_detector_distance_destroy(res->handle);
    res->handle = NULL;

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
