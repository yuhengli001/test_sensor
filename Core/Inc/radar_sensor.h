#ifndef RADAR_SENSOR_H
#define RADAR_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Software-only initialization of the Acconeer A121 Sensor resources.
 * This function handles memory allocation and configuration setup, but does 
 * NOT power on the physical sensor.
 * @retval true if successful, false otherwise
 */
bool Radar_Sensor_PreInit(void);

/**
 * @brief Power on the physical sensor, enable GPIOs, and perform calibration.
 * This should be called once a BLE connection is established.
 * @retval true if hardware initialization and calibration succeed, false otherwise
 */
bool Radar_Sensor_Start(void);

/**
 * @brief Run a single measurement and fetch the distance.
 * This function requires Radar_Sensor_Start() to have been called.
 * @param distance_mm Pointer to store the closest measured distance (in mm)
 * @param num_targets Pointer to store how many distinct targets were seen
 * @retval true if measurement was successful, false if it failed or timed out
 */
bool Radar_Sensor_Get_Next(uint16_t *distance_mm, uint8_t *num_targets);

/**
 * @brief Power down the physical sensor and disable GPIOs.
 * This should be called once a BLE connection is terminated.
 */
void Radar_Sensor_Stop(void);

/**
 * @brief Deallocate all software resources and cleanup memory.
 */
void Radar_Sensor_Cleanup(void);

#endif /* RADAR_SENSOR_H */
