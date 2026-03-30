#include "radar_sensor.h"

#include <stdio.h>
#include <stdlib.h>

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

static void cleanup(distance_detector_resources_t *resources);
static void set_config(acc_detector_distance_config_t *detector_config, distance_preset_config_t preset);
static bool initialize_detector_resources(distance_detector_resources_t *resources);
static bool do_sensor_calibration(acc_sensor_t *sensor, acc_cal_result_t *sensor_cal_result, void *buffer, uint32_t buffer_size);
static bool do_full_detector_calibration(distance_detector_resources_t *resources, const acc_cal_result_t *sensor_cal_result);
static bool do_detector_calibration_update(distance_detector_resources_t *resources, const acc_cal_result_t *sensor_cal_result);
static bool do_detector_get_next(distance_detector_resources_t  *resources, const acc_cal_result_t *sensor_cal_result, acc_detector_distance_result_t *result);

bool Radar_Sensor_Init(void)
{
    if (initialized) return true;

    const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();
    if (!acc_rss_hal_register(hal)) {
        return false;
    }

    resources.config = acc_detector_distance_config_create();
    if (resources.config == NULL) {
        return false;
    }

    set_config(resources.config, DISTANCE_PRESET_CONFIG_BALANCED);

    if (!initialize_detector_resources(&resources)) {
        cleanup(&resources);
        return false;
    }

    acc_hal_integration_sensor_supply_on(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    resources.sensor = acc_sensor_create(SENSOR_ID);
    if (resources.sensor == NULL) {
        cleanup(&resources);
        return false;
    }

    if (!do_sensor_calibration(resources.sensor, &sensor_cal_result, resources.buffer, resources.buffer_size)) {
        cleanup(&resources);
        return false;
    }

    if (!do_full_detector_calibration(&resources, &sensor_cal_result)) {
        cleanup(&resources);
        return false;
    }

    initialized = true;
    return true;
}

bool Radar_Sensor_Get_Next(uint16_t *distance_mm, uint8_t *num_targets)
{
    if (!initialized) return false;

    acc_detector_distance_result_t result = {0};

    if (!do_detector_get_next(&resources, &sensor_cal_result, &result)) {
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
    if (result.num_distances > 0) {
        /* Convert float meters to uint16_t millimeters */
        *distance_mm = (uint16_t)(result.distances[0] * 1000.0f);
    } else {
        *distance_mm = 0;
    }

    return true;
}

void Radar_Sensor_Cleanup(void)
{
    if (!initialized) return;
    cleanup(&resources);
    initialized = false;
}

/* -------------------------------------------------------------------------- */
/* HELPER FUNCTIONS (Migrated directly from example_detector_distance.c)      */
/* -------------------------------------------------------------------------- */

static void cleanup(distance_detector_resources_t *res)
{
	acc_hal_integration_sensor_disable(SENSOR_ID);
	acc_hal_integration_sensor_supply_off(SENSOR_ID);

	acc_detector_distance_config_destroy(res->config);
	acc_detector_distance_destroy(res->handle);

	acc_integration_mem_free(res->buffer);
	acc_integration_mem_free(res->detector_cal_result_static);

	if (res->sensor != NULL)
	{
		acc_sensor_destroy(res->sensor);
	}
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
			// ... 
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
		if (!acc_detector_distance_prepare(res->handle, res->config, res->sensor, sensor_cal_res, res->buffer, res->buffer_size)) return false;
		if (!acc_sensor_measure(res->sensor)) return false;
		if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS)) return false;
		if (!acc_sensor_read(res->sensor, res->buffer, res->buffer_size)) return false;

		if (!acc_detector_distance_process(res->handle, res->buffer, res->detector_cal_result_static,
		                                   &res->detector_cal_result_dynamic, &result_available, result))
		{
			return false;
		}
	} while (!result_available);

	return true;
}
