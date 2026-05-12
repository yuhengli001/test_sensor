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
  float frequency;     /* Hz */
  float displacement;  /* um */
  float velocity;      /* mm/s */
  float acceleration;  /* m/s2 */
} VIBRATION_Data_t;

typedef struct __PACKED
{
  /* Basic Settings */
  uint32_t measured_point;
  float    amplitude_threshold;
  float    threshold_margin_um;
  uint8_t  displacement_mode; /* 0: Amplitude, 1: Peak-to-Peak */
  
  /* Advanced Settings */
  uint8_t  profile;
  float    frame_rate_hz;
  uint8_t  frame_rate_limit;
  float    sweep_rate_hz;
  uint16_t sweeps_per_frame;
  uint16_t hwaas;
  uint8_t  double_buffering;
  uint8_t  continuous_sweep_mode;
  uint8_t  inter_frame_idle_state;
  uint8_t  inter_sweep_idle_state;
  uint32_t time_series_length;
  float    time_filtering_coefficient;
  uint8_t  low_frequency_enhancement;
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
void VIBRATION_APP_UpdateData(float freq, float displ, float vel, float accel);
VIBRATION_Config_t* VIBRATION_APP_GetConfig(void);
/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*VIBRATION_SERVICE_APP_H */
