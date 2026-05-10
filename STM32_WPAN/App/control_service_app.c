/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Control_Service_app.c
  * @author  MCD Application Team
  * @brief   Control_Service_app application definition.
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
#include "control_service_app.h"
#include "control_service.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef enum
{
  Sensor_status_NOTIFICATION_OFF,
  Sensor_status_NOTIFICATION_ON,
  /* USER CODE BEGIN Service1_APP_SendInformation_t */

  /* USER CODE END Service1_APP_SendInformation_t */
  CONTROL_SERVICE_APP_SENDINFORMATION_LAST
} CONTROL_SERVICE_APP_SendInformation_t;

typedef struct
{
  CONTROL_SERVICE_APP_SendInformation_t     Sensor_status_Notification_Status;
  /* USER CODE BEGIN Service1_APP_Context_t */

  /* USER CODE END Service1_APP_Context_t */
  uint16_t              ConnectionHandle;
} CONTROL_SERVICE_APP_Context_t;

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
static CONTROL_SERVICE_APP_Context_t CONTROL_SERVICE_APP_Context;

uint8_t a_CONTROL_SERVICE_UpdateCharData[247];

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void CONTROL_SERVICE_Sensor_status_SendNotification(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void CONTROL_SERVICE_Notification(CONTROL_SERVICE_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_Notification_1 */

  /* USER CODE END Service1_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_Notification_Service1_EvtOpcode */

    /* USER CODE END Service1_Notification_Service1_EvtOpcode */

    case CONTROL_SERVICE_ACTIVE_MODE_READ_EVT:
      /* USER CODE BEGIN Service1Char1_READ_EVT */

      /* USER CODE END Service1Char1_READ_EVT */
      break;

    case CONTROL_SERVICE_ACTIVE_MODE_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN Service1Char1_WRITE_NO_RESP_EVT */

      /* USER CODE END Service1Char1_WRITE_NO_RESP_EVT */
      break;

    case CONTROL_SERVICE_SYSTEM_COMMAND_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN Service1Char2_WRITE_NO_RESP_EVT */

      /* USER CODE END Service1Char2_WRITE_NO_RESP_EVT */
      break;

    case CONTROL_SERVICE_SENSOR_STATUS_READ_EVT:
      /* USER CODE BEGIN Service1Char3_READ_EVT */

      /* USER CODE END Service1Char3_READ_EVT */
      break;

    case CONTROL_SERVICE_SENSOR_STATUS_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char3_NOTIFY_ENABLED_EVT */

      /* USER CODE END Service1Char3_NOTIFY_ENABLED_EVT */
      break;

    case CONTROL_SERVICE_SENSOR_STATUS_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char3_NOTIFY_DISABLED_EVT */

      /* USER CODE END Service1Char3_NOTIFY_DISABLED_EVT */
      break;

    default:
      /* USER CODE BEGIN Service1_Notification_default */

      /* USER CODE END Service1_Notification_default */
      break;
  }
  /* USER CODE BEGIN Service1_Notification_2 */

  /* USER CODE END Service1_Notification_2 */
  return;
}

void CONTROL_SERVICE_APP_EvtRx(CONTROL_SERVICE_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_APP_EvtRx_1 */

  /* USER CODE END Service1_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_APP_EvtRx_Service1_EvtOpcode */

    /* USER CODE END Service1_APP_EvtRx_Service1_EvtOpcode */
    case CONTROL_SERVICE_CONN_HANDLE_EVT :
      /* USER CODE BEGIN Service1_APP_CONN_HANDLE_EVT */

      /* USER CODE END Service1_APP_CONN_HANDLE_EVT */
      break;

    case CONTROL_SERVICE_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN Service1_APP_DISCON_HANDLE_EVT */

      /* USER CODE END Service1_APP_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN Service1_APP_EvtRx_default */

      /* USER CODE END Service1_APP_EvtRx_default */
      break;
  }

  /* USER CODE BEGIN Service1_APP_EvtRx_2 */

  /* USER CODE END Service1_APP_EvtRx_2 */

  return;
}

void CONTROL_SERVICE_APP_Init(void)
{
  UNUSED(CONTROL_SERVICE_APP_Context);
  CONTROL_SERVICE_Init();

  /* USER CODE BEGIN Service1_APP_Init */

  /* USER CODE END Service1_APP_Init */
  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
__USED void CONTROL_SERVICE_Sensor_status_SendNotification(void) /* Property Notification */
{
  CONTROL_SERVICE_APP_SendInformation_t notification_on_off = Sensor_status_NOTIFICATION_OFF;
  CONTROL_SERVICE_Data_t control_service_notification_data;

  control_service_notification_data.p_Payload = (uint8_t*)a_CONTROL_SERVICE_UpdateCharData;
  control_service_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char3_NS_1 */

  /* USER CODE END Service1Char3_NS_1 */

  if (notification_on_off != Sensor_status_NOTIFICATION_OFF)
  {
    CONTROL_SERVICE_UpdateValue(CONTROL_SERVICE_SENSOR_STATUS, &control_service_notification_data);
  }

  /* USER CODE BEGIN Service1Char3_NS_Last */

  /* USER CODE END Service1Char3_NS_Last */

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */

/* USER CODE END FD_LOCAL_FUNCTIONS */
