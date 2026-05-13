/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Fall_Service_app.c
  * @author  MCD Application Team
  * @brief   Fall_Service_app application definition.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_common.h"
#include "log_module.h"
#include "app_ble.h"
#include "ll_sys_if.h"
#include "dbg_trace.h"
#include "fall_service_app.h"
#include "fall_service.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef enum
{
  Fall_data_NOTIFICATION_OFF,
  Fall_data_NOTIFICATION_ON,
  /* USER CODE BEGIN Service3_APP_SendInformation_t */

  /* USER CODE END Service3_APP_SendInformation_t */
  FALL_SERVICE_APP_SENDINFORMATION_LAST
} FALL_SERVICE_APP_SendInformation_t;

typedef struct
{
  FALL_SERVICE_APP_SendInformation_t     Fall_data_Notification_Status;
  /* USER CODE BEGIN Service3_APP_Context_t */

  /* USER CODE END Service3_APP_Context_t */
  uint16_t              ConnectionHandle;
} FALL_SERVICE_APP_Context_t;

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static FALL_SERVICE_APP_Context_t FALL_SERVICE_APP_Context;

uint8_t a_FALL_SERVICE_UpdateCharData[247];

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void FALL_SERVICE_Fall_data_SendNotification(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void FALL_SERVICE_Notification(FALL_SERVICE_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service3_Notification_1 */

  /* USER CODE END Service3_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service3_Notification_Service3_EvtOpcode */

    /* USER CODE END Service3_Notification_Service3_EvtOpcode */

    case FALL_SERVICE_FALL_CONFIG_READ_EVT:
      /* USER CODE BEGIN Service3Char1_READ_EVT */

      /* USER CODE END Service3Char1_READ_EVT */
      break;

    case FALL_SERVICE_FALL_CONFIG_WRITE_EVT:
      /* USER CODE BEGIN Service3Char1_WRITE_EVT */

      /* USER CODE END Service3Char1_WRITE_EVT */
      break;

    case FALL_SERVICE_FALL_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service3Char2_NOTIFY_ENABLED_EVT */
      FALL_SERVICE_APP_Context.Fall_data_Notification_Status = Fall_data_NOTIFICATION_ON;
      /* USER CODE END Service3Char2_NOTIFY_ENABLED_EVT */
      break;

    case FALL_SERVICE_FALL_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service3Char2_NOTIFY_DISABLED_EVT */
      FALL_SERVICE_APP_Context.Fall_data_Notification_Status = Fall_data_NOTIFICATION_OFF;
      /* USER CODE END Service3Char2_NOTIFY_DISABLED_EVT */
      break;

    default:
      /* USER CODE BEGIN Service3_Notification_default */

      /* USER CODE END Service3_Notification_default */
      break;
  }
  /* USER CODE BEGIN Service3_Notification_2 */

  /* USER CODE END Service3_Notification_2 */
  return;
}

void FALL_SERVICE_APP_EvtRx(FALL_SERVICE_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service3_APP_EvtRx_1 */

  /* USER CODE END Service3_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service3_APP_EvtRx_Service3_EvtOpcode */

    /* USER CODE END Service3_APP_EvtRx_Service3_EvtOpcode */
    case FALL_SERVICE_CONN_HANDLE_EVT :
      /* USER CODE BEGIN Service3_APP_CONN_HANDLE_EVT */

      /* USER CODE END Service3_APP_CONN_HANDLE_EVT */
      break;

    case FALL_SERVICE_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN Service3_APP_DISCON_HANDLE_EVT */

      /* USER CODE END Service3_APP_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN Service3_APP_EvtRx_default */

      /* USER CODE END Service3_APP_EvtRx_default */
      break;
  }

  /* USER CODE BEGIN Service3_APP_EvtRx_2 */

  /* USER CODE END Service3_APP_EvtRx_2 */

  return;
}

void FALL_SERVICE_APP_Init(void)
{
  UNUSED(FALL_SERVICE_APP_Context);
  FALL_SERVICE_Init();

  /* USER CODE BEGIN Service3_APP_Init */

  /* USER CODE END Service3_APP_Init */
  return;
}

/* USER CODE BEGIN FD */
void FALL_APP_UpdateData(uint8_t fall_status, float velocity, float distance)
{
  if(FALL_SERVICE_APP_Context.Fall_data_Notification_Status == Fall_data_NOTIFICATION_ON)
  {
    uint8_t payload[9];
    payload[0] = fall_status;
    memcpy(&payload[1], &velocity, 4);
    memcpy(&payload[5], &distance, 4);

    FALL_SERVICE_Data_t fall_service_notification_data;
    fall_service_notification_data.p_Payload = payload;
    fall_service_notification_data.Length = 9;

    FALL_SERVICE_UpdateValue(FALL_SERVICE_FALL_DATA, &fall_service_notification_data);
  }
}
/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
__USED void FALL_SERVICE_Fall_data_SendNotification(void) /* Property Notification */
{
  FALL_SERVICE_APP_SendInformation_t notification_on_off = Fall_data_NOTIFICATION_OFF;
  FALL_SERVICE_Data_t fall_service_notification_data;

  fall_service_notification_data.p_Payload = (uint8_t*)a_FALL_SERVICE_UpdateCharData;
  fall_service_notification_data.Length = 0;

  /* USER CODE BEGIN Service3Char2_NS_1 */

  /* USER CODE END Service3Char2_NS_1 */

  if (notification_on_off != Fall_data_NOTIFICATION_OFF)
  {
    FALL_SERVICE_UpdateValue(FALL_SERVICE_FALL_DATA, &fall_service_notification_data);
  }

  /* USER CODE BEGIN Service3Char2_NS_Last */

  /* USER CODE END Service3Char2_NS_Last */

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */

/* USER CODE END FD_LOCAL_FUNCTIONS */
