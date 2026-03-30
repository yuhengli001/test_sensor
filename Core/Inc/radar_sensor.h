#ifndef RADAR_SENSOR_H
#define RADAR_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the Acconeer A121 Sensor and Distance Detector
 * @retval true if successful, false otherwise
 */
bool Radar_Sensor_Init(void);

/**
 * @brief Run a single measurement and fetch the distance
 * @param distance_mm Pointer to store the closest measured distance (in mm)
 * @param num_targets Pointer to store how many distinct targets were seen
 * @retval true if measurement was successful, false if it failed or timed out
 */
bool Radar_Sensor_Get_Next(uint16_t *distance_mm, uint8_t *num_targets);

/**
 * @brief Power down the sensor and free the memory
 */
void Radar_Sensor_Cleanup(void);

#endif /* RADAR_SENSOR_H */
