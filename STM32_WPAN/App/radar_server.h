/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Radar_Server.h
  * @author  MCD Application Team
  * @brief   Header for Radar_Server.c
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
#ifndef RADAR_SERVER_H
#define RADAR_SERVER_H

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
  RADAR_SERVER_A121_CONTROL,
  RADAR_SERVER_A121_DATA,
  /* USER CODE BEGIN Service1_CharOpcode_t */

  /* USER CODE END Service1_CharOpcode_t */
  RADAR_SERVER_CHAROPCODE_LAST
} RADAR_SERVER_CharOpcode_t;

typedef enum
{
  RADAR_SERVER_A121_CONTROL_READ_EVT,
  RADAR_SERVER_A121_CONTROL_WRITE_NO_RESP_EVT,
  RADAR_SERVER_A121_DATA_NOTIFY_ENABLED_EVT,
  RADAR_SERVER_A121_DATA_NOTIFY_DISABLED_EVT,
  /* USER CODE BEGIN Service1_OpcodeEvt_t */

  /* USER CODE END Service1_OpcodeEvt_t */
  RADAR_SERVER_BOOT_REQUEST_EVT
} RADAR_SERVER_OpcodeEvt_t;

typedef struct
{
  uint8_t *p_Payload;
  uint8_t Length;

  /* USER CODE BEGIN Service1_Data_t */

  /* USER CODE END Service1_Data_t */
} RADAR_SERVER_Data_t;

typedef struct
{
  RADAR_SERVER_OpcodeEvt_t       EvtOpcode;
  RADAR_SERVER_Data_t             DataTransfered;
  uint16_t                ConnectionHandle;
  uint16_t                AttributeHandle;
  uint8_t                 ServiceInstance;
  /* USER CODE BEGIN Service1_NotificationEvt_t */

  /* USER CODE END Service1_NotificationEvt_t */
} RADAR_SERVER_NotificationEvt_t;

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
void RADAR_SERVER_Init(void);
void RADAR_SERVER_Notification(RADAR_SERVER_NotificationEvt_t *p_Notification);
tBleStatus RADAR_SERVER_UpdateValue(RADAR_SERVER_CharOpcode_t CharOpcode, RADAR_SERVER_Data_t *pData);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*RADAR_SERVER_H */
