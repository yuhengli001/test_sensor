#ifndef RADAR_ADAPTER_H
#define RADAR_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>

/* Matches control_service_app.c modes */
typedef enum {
    RADAR_MODE_NONE      = 0,
    RADAR_MODE_VITAL     = 1,
    RADAR_MODE_FALL      = 2,
    RADAR_MODE_VIBRATION = 3
} Radar_Mode_t;

/**
 * @brief Initialize the Radar RSS and HAL. Call once at boot.
 * 
 * @return true if the initialization was successful, false otherwise.
 */
bool Radar_Adapter_Init(void);

/**
 * @brief Start the sensor for a specific mode. Cleans up old mode first.
 * @param mode The mode to start.
 * 
 * @return true if the initialization was successful, false otherwise.
 */
bool Radar_Adapter_Start(Radar_Mode_t mode);

/**
 * @brief Perform one measurement and dispatch data to the appropriate BLE service.
 * @param mode The current mode.
 * 
 * @return true if the measurement was successful, false otherwise.
 */
bool Radar_Adapter_Process(Radar_Mode_t mode);

/**
 * @brief Stop the sensor and free memory buffers. Call when hitting "STOP".
 * 
 */
void Radar_Adapter_Stop(void);

#endif // RADAR_ADAPTER_H
