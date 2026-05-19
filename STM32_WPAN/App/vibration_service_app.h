/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Vibration_Service_app.h
  * @author  MCD Application Team
  * @brief   Header for Vibration_Service_app.c
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef VIBRATION_SERVICE_APP_H
#define VIBRATION_SERVICE_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ble_types.h"
#include "ble_core.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  VIBRATION_SERVICE_CONN_HANDLE_EVT,
  VIBRATION_SERVICE_DISCON_HANDLE_EVT,

  /* USER CODE BEGIN Service4_OpcodeNotificationEvt_t */

  /* USER CODE END Service4_OpcodeNotificationEvt_t */

  VIBRATION_SERVICE_LAST_EVT,
} VIBRATION_SERVICE_APP_OpcodeNotificationEvt_t;

typedef struct
{
  VIBRATION_SERVICE_APP_OpcodeNotificationEvt_t          EvtOpcode;
  uint16_t                                 ConnectionHandle;

  /* USER CODE BEGIN VIBRATION_SERVICE_APP_ConnHandleNotEvt_t */

  /* USER CODE END VIBRATION_SERVICE_APP_ConnHandleNotEvt_t */
} VIBRATION_SERVICE_APP_ConnHandleNotEvt_t;
/* USER CODE BEGIN ET */
typedef struct __PACKED
{
  float frequency;        /* Hz */
  float displacement;     /* um (peak) */
  float displacement_rms; /* um (RMS) */
  float velocity;         /* mm/s (peak) */
  float velocity_rms;     /* mm/s (RMS) */
  float acceleration;     /* m/s^2 (peak) */
  float acceleration_rms; /* m/s^2 (RMS) */
} VIBRATION_Data_t;

typedef struct __PACKED
{
  uint8_t  preset;         /* 0: HIGH_FREQUENCY (10-5000 Hz), 1: LOW_FREQUENCY (0.1-100 Hz) */
  uint32_t measured_point; /* distance index; distance_mm = measured_point * 2.5 */
} VIBRATION_Config_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Exported macros -----------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void VIBRATION_SERVICE_APP_Init(void);
void VIBRATION_SERVICE_APP_EvtRx(VIBRATION_SERVICE_APP_ConnHandleNotEvt_t *p_Notification);
/* USER CODE BEGIN EFP */
void VIBRATION_APP_UpdateData(float freq, float displ, float displ_rms, float vel, float vel_rms, float accel, float accel_rms);
VIBRATION_Config_t* VIBRATION_APP_GetConfig(void);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*VIBRATION_SERVICE_APP_H */
