#ifndef RADAR_SENSOR_H
#define RADAR_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Parameter IDs for real-time tuning via BLE
 */
typedef enum {
    RADAR_PARAM_RANGE_START      = 0x01,
    RADAR_PARAM_RANGE_END        = 0x02,
    RADAR_PARAM_SENSITIVITY      = 0x03,
    RADAR_PARAM_MAX_PROFILE      = 0x04,
    RADAR_PARAM_SIGNAL_QUALITY   = 0x05,
    RADAR_PARAM_MAX_STEP_LENGTH  = 0x06,
    RADAR_PARAM_PEAK_SORTING     = 0x07,
    RADAR_PARAM_THRESH_METHOD    = 0x08,
    RADAR_PARAM_REFLECTOR_SHAPE  = 0x09,
    RADAR_PARAM_LEAKAGE_CANCEL   = 0x0A,
    RADAR_PARAM_NUM_FRAMES       = 0x0B,
    RADAR_PARAM_FIXED_AMP_THR    = 0x0C,
    RADAR_PARAM_FIXED_STR_THR    = 0x0D,
    RADAR_PARAM_TRACK_MIN_DIST   = 0x0E,
    RADAR_PARAM_TRACK_MAX_DIST   = 0x0F,
    RADAR_PARAM_TRACK_STRENGTH   = 0x10,
    RADAR_PARAM_TRACK_GATE       = 0x11,
} radar_param_id_t;

/**
 * @brief Software-only initialization of the Acconeer A121 Sensor resources.
 * @retval true if successful, false otherwise
 */
bool Radar_Sensor_PreInit(void);

/**
 * @brief Update a specific configuration parameter.
 * If the sensor is already running, it will automatically restart with the new config.
 * @param param_id The ID of the parameter to update
 * @param value Pointer to the new value (float or uint32_t)
 * @retval true if successful
 */
bool Radar_Sensor_UpdateParam(radar_param_id_t param_id, void *value);

/**
 * @brief Power on the physical sensor and perform calibration.
 * @retval true if successful
 */
bool Radar_Sensor_Start(void);

/**
 * @brief Run a single measurement and fetch results.
 * @param distances_m Array of measured distances (in meters)
 * @param strengths_db Array of measured strengths (in dB)
 * @param num_targets Pointer to store number of targets found
 * @retval true if successful
 */
bool Radar_Sensor_Get_Next_Results(float *distances_m, float *strengths_db, uint8_t *num_targets);

/**
 * @brief Get whether a target is currently locked.
 * @param distance_m Pointer to store tracked distance
 * @param strength_db Pointer to store tracked strength
 * @retval true if a target is locked
 */
bool Radar_Sensor_Get_Tracked_State(float *distance_m, float *strength_db);

/**
 * @brief Stop the physical sensor and free temporary resources.
 */
void Radar_Sensor_Stop(void);

/**
 * @brief Deallocate all software resources.
 */
void Radar_Sensor_Cleanup(void);

#endif /* RADAR_SENSOR_H */
