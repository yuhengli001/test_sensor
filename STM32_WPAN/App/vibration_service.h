/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Vibration_Service.h
  * @author  MCD Application Team
  * @brief   Header for Vibration_Service.c
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
#ifndef VIBRATION_SERVICE_H
#define VIBRATION_SERVICE_H

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
  VIBRATION_SERVICE_VIBRATION_CONFIG,
  VIBRATION_SERVICE_VIBRATION_DATA,
  VIBRATION_SERVICE_SPECTRUM_ARRAY,
  /* USER CODE BEGIN Service4_CharOpcode_t */

  /* USER CODE END Service4_CharOpcode_t */
  VIBRATION_SERVICE_CHAROPCODE_LAST
} VIBRATION_SERVICE_CharOpcode_t;

typedef enum
{
  VIBRATION_SERVICE_VIBRATION_CONFIG_READ_EVT,
  VIBRATION_SERVICE_VIBRATION_CONFIG_WRITE_EVT,
  VIBRATION_SERVICE_VIBRATION_DATA_NOTIFY_ENABLED_EVT,
  VIBRATION_SERVICE_VIBRATION_DATA_NOTIFY_DISABLED_EVT,
  VIBRATION_SERVICE_SPECTRUM_ARRAY_NOTIFY_ENABLED_EVT,
  VIBRATION_SERVICE_SPECTRUM_ARRAY_NOTIFY_DISABLED_EVT,
  /* USER CODE BEGIN Service4_OpcodeEvt_t */

  /* USER CODE END Service4_OpcodeEvt_t */
  VIBRATION_SERVICE_BOOT_REQUEST_EVT
} VIBRATION_SERVICE_OpcodeEvt_t;

typedef struct
{
  uint8_t *p_Payload;
  uint8_t Length;

  /* USER CODE BEGIN Service4_Data_t */

  /* USER CODE END Service4_Data_t */
} VIBRATION_SERVICE_Data_t;

typedef struct
{
  VIBRATION_SERVICE_OpcodeEvt_t       EvtOpcode;
  VIBRATION_SERVICE_Data_t             DataTransfered;
  uint16_t                ConnectionHandle;
  uint16_t                AttributeHandle;
  uint8_t                 ServiceInstance;
  /* USER CODE BEGIN Service4_NotificationEvt_t */

  /* USER CODE END Service4_NotificationEvt_t */
} VIBRATION_SERVICE_NotificationEvt_t;

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
void VIBRATION_SERVICE_Init(void);
void VIBRATION_SERVICE_Notification(VIBRATION_SERVICE_NotificationEvt_t *p_Notification);
tBleStatus VIBRATION_SERVICE_UpdateValue(VIBRATION_SERVICE_CharOpcode_t CharOpcode, VIBRATION_SERVICE_Data_t *pData);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*VIBRATION_SERVICE_H */
