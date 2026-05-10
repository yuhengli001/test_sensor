/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Fall_Service.h
  * @author  MCD Application Team
  * @brief   Header for Fall_Service.c
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
#ifndef FALL_SERVICE_H
#define FALL_SERVICE_H

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
  FALL_SERVICE_FALL_CONFIG,
  FALL_SERVICE_FALL_DATA,
  /* USER CODE BEGIN Service3_CharOpcode_t */

  /* USER CODE END Service3_CharOpcode_t */
  FALL_SERVICE_CHAROPCODE_LAST
} FALL_SERVICE_CharOpcode_t;

typedef enum
{
  FALL_SERVICE_FALL_CONFIG_READ_EVT,
  FALL_SERVICE_FALL_CONFIG_WRITE_EVT,
  FALL_SERVICE_FALL_DATA_NOTIFY_ENABLED_EVT,
  FALL_SERVICE_FALL_DATA_NOTIFY_DISABLED_EVT,
  /* USER CODE BEGIN Service3_OpcodeEvt_t */

  /* USER CODE END Service3_OpcodeEvt_t */
  FALL_SERVICE_BOOT_REQUEST_EVT
} FALL_SERVICE_OpcodeEvt_t;

typedef struct
{
  uint8_t *p_Payload;
  uint8_t Length;

  /* USER CODE BEGIN Service3_Data_t */

  /* USER CODE END Service3_Data_t */
} FALL_SERVICE_Data_t;

typedef struct
{
  FALL_SERVICE_OpcodeEvt_t       EvtOpcode;
  FALL_SERVICE_Data_t             DataTransfered;
  uint16_t                ConnectionHandle;
  uint16_t                AttributeHandle;
  uint8_t                 ServiceInstance;
  /* USER CODE BEGIN Service3_NotificationEvt_t */

  /* USER CODE END Service3_NotificationEvt_t */
} FALL_SERVICE_NotificationEvt_t;

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
void FALL_SERVICE_Init(void);
void FALL_SERVICE_Notification(FALL_SERVICE_NotificationEvt_t *p_Notification);
tBleStatus FALL_SERVICE_UpdateValue(FALL_SERVICE_CharOpcode_t CharOpcode, FALL_SERVICE_Data_t *pData);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*FALL_SERVICE_H */
