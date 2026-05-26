// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#ifndef RADAR_SENSOR_H
#define RADAR_SENSOR_H

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Parameter IDs for real-time tuning via BLE
 */
typedef enum
{
	RADAR_PARAM_RANGE_START     = 0x01,
	RADAR_PARAM_RANGE_END       = 0x02,
	RADAR_PARAM_SENSITIVITY     = 0x03,
	RADAR_PARAM_MAX_PROFILE     = 0x04,
	RADAR_PARAM_SIGNAL_QUALITY  = 0x05,
	RADAR_PARAM_MAX_STEP_LENGTH = 0x06,
	RADAR_PARAM_PEAK_SORTING    = 0x07,
	RADAR_PARAM_THRESH_METHOD   = 0x08,
	RADAR_PARAM_REFLECTOR_SHAPE = 0x09,
	RADAR_PARAM_LEAKAGE_CANCEL  = 0x0A,
	RADAR_PARAM_NUM_FRAMES      = 0x0B,
	RADAR_PARAM_FIXED_AMP_THR   = 0x0C,
	RADAR_PARAM_FIXED_STR_THR   = 0x0D,

	// Software-only tracking filter parameters
	RADAR_PARAM_TRACK_MIN_DIST  = 0x0E,
	RADAR_PARAM_TRACK_MAX_DIST  = 0x0F,
	RADAR_PARAM_TRACK_STRENGTH  = 0x10,
	RADAR_PARAM_TRACK_GATE      = 0x11,
} radar_param_id_t;


/**
 * @brief Software-only initialization of the Acconeer A121 sensor resources.
 *        Must be called before Radar_Sensor_Start().
 * @retval true on success
 */
bool Radar_Sensor_PreInit(void);


/**
 * @brief Update a specific configuration parameter.
 *        Hardware parameters trigger a reconfigure; software parameters take effect immediately.
 * @param param_id  ID of the parameter to update
 * @param value     Pointer to the new value (float* or uint32_t* depending on parameter)
 * @retval true on success
 */
bool Radar_Sensor_UpdateParam(radar_param_id_t param_id, void *value);


/**
 * @brief Power on the physical sensor and perform full calibration.
 * @retval true on success
 */
bool Radar_Sensor_Start(void);


/**
 * @brief Run one measurement cycle and return the tracked target result.
 * @param distances_m   Output array of distances in meters
 * @param strengths_db  Output array of signal strengths in dB
 * @param num_targets   Output number of targets found (0 or 1)
 * @retval true on success
 */
bool Radar_Sensor_Get_Next_Results(float *distances_m, float *strengths_db, uint8_t *num_targets);


/**
 * @brief Query the current tracking filter state without triggering a new measurement.
 * @param distance_m   Output tracked distance in meters (may be NULL)
 * @param strength_db  Output tracked signal strength in dB (may be NULL)
 * @retval true if a target is currently locked
 */
bool Radar_Sensor_Get_Tracked_State(float *distance_m, float *strength_db);


/**
 * @brief Power off the physical sensor and release temporary resources.
 */
void Radar_Sensor_Stop(void);


/**
 * @brief Release all software resources allocated by Radar_Sensor_PreInit().
 */
void Radar_Sensor_Cleanup(void);


#endif // RADAR_SENSOR_H
