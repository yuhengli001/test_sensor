/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Vital_Sign_Service.h
  * @author  MCD Application Team
  * @brief   Header for Vital_Sign_Service.c
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
#ifndef VITAL_SIGN_SERVICE_H
#define VITAL_SIGN_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ble_types.h"
#include "ble_core.h"
#include "svc_ctl.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported defines ----------------------------------------------------------*/
/* USER CODE BEGIN ED */

/* USER CODE END ED */

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  VITAL_SIGN_SERVICE_DISTANCE_CONFIG,
  VITAL_SIGN_SERVICE_VITAL_DATA,
  VITAL_SIGN_SERVICE_WAVEFORM,
  /* USER CODE BEGIN Service2_CharOpcode_t */

  /* USER CODE END Service2_CharOpcode_t */
  VITAL_SIGN_SERVICE_CHAROPCODE_LAST
} VITAL_SIGN_SERVICE_CharOpcode_t;

typedef enum
{
  VITAL_SIGN_SERVICE_DISTANCE_CONFIG_READ_EVT,
  VITAL_SIGN_SERVICE_DISTANCE_CONFIG_WRITE_EVT,
  VITAL_SIGN_SERVICE_VITAL_DATA_NOTIFY_ENABLED_EVT,
  VITAL_SIGN_SERVICE_VITAL_DATA_NOTIFY_DISABLED_EVT,
  VITAL_SIGN_SERVICE_WAVEFORM_NOTIFY_ENABLED_EVT,
  VITAL_SIGN_SERVICE_WAVEFORM_NOTIFY_DISABLED_EVT,
  /* USER CODE BEGIN Service2_OpcodeEvt_t */

  /* USER CODE END Service2_OpcodeEvt_t */
  VITAL_SIGN_SERVICE_BOOT_REQUEST_EVT
} VITAL_SIGN_SERVICE_OpcodeEvt_t;

typedef struct
{
  uint8_t *p_Payload;
  uint8_t Length;

  /* USER CODE BEGIN Service2_Data_t */

  /* USER CODE END Service2_Data_t */
} VITAL_SIGN_SERVICE_Data_t;

typedef struct
{
  VITAL_SIGN_SERVICE_OpcodeEvt_t       EvtOpcode;
  VITAL_SIGN_SERVICE_Data_t             DataTransfered;
  uint16_t                ConnectionHandle;
  uint16_t                AttributeHandle;
  uint8_t                 ServiceInstance;
  /* USER CODE BEGIN Service2_NotificationEvt_t */

  /* USER CODE END Service2_NotificationEvt_t */
} VITAL_SIGN_SERVICE_NotificationEvt_t;

/* USER CODE BEGIN ET */

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
void VITAL_SIGN_SERVICE_Init(void);
void VITAL_SIGN_SERVICE_Notification(VITAL_SIGN_SERVICE_NotificationEvt_t *p_Notification);
tBleStatus VITAL_SIGN_SERVICE_UpdateValue(VITAL_SIGN_SERVICE_CharOpcode_t CharOpcode, VITAL_SIGN_SERVICE_Data_t *pData);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*VITAL_SIGN_SERVICE_H */
