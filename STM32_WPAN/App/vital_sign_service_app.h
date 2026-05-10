/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Vital_Sign_Service_app.h
  * @author  MCD Application Team
  * @brief   Header for Vital_Sign_Service_app.c
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
#ifndef VITAL_SIGN_SERVICE_APP_H
#define VITAL_SIGN_SERVICE_APP_H

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
  VITAL_SIGN_SERVICE_CONN_HANDLE_EVT,
  VITAL_SIGN_SERVICE_DISCON_HANDLE_EVT,

  /* USER CODE BEGIN Service2_OpcodeNotificationEvt_t */

  /* USER CODE END Service2_OpcodeNotificationEvt_t */

  VITAL_SIGN_SERVICE_LAST_EVT,
} VITAL_SIGN_SERVICE_APP_OpcodeNotificationEvt_t;

typedef struct
{
  VITAL_SIGN_SERVICE_APP_OpcodeNotificationEvt_t          EvtOpcode;
  uint16_t                                 ConnectionHandle;

  /* USER CODE BEGIN VITAL_SIGN_SERVICE_APP_ConnHandleNotEvt_t */

  /* USER CODE END VITAL_SIGN_SERVICE_APP_ConnHandleNotEvt_t */
} VITAL_SIGN_SERVICE_APP_ConnHandleNotEvt_t;
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
void VITAL_SIGN_SERVICE_APP_Init(void);
void VITAL_SIGN_SERVICE_APP_EvtRx(VITAL_SIGN_SERVICE_APP_ConnHandleNotEvt_t *p_Notification);
/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /*VITAL_SIGN_SERVICE_APP_H */
