/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Vital_Sign_Service_app.c
  * @author  MCD Application Team
  * @brief   Vital_Sign_Service_app application definition.
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
#include "vital_sign_service_app.h"
#include "vital_sign_service.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef enum
{
  Vital_data_NOTIFICATION_OFF,
  Vital_data_NOTIFICATION_ON,
  Waveform_NOTIFICATION_OFF,
  Waveform_NOTIFICATION_ON,
  /* USER CODE BEGIN Service2_APP_SendInformation_t */

  /* USER CODE END Service2_APP_SendInformation_t */
  VITAL_SIGN_SERVICE_APP_SENDINFORMATION_LAST
} VITAL_SIGN_SERVICE_APP_SendInformation_t;

typedef struct
{
  VITAL_SIGN_SERVICE_APP_SendInformation_t     Vital_data_Notification_Status;
  VITAL_SIGN_SERVICE_APP_SendInformation_t     Waveform_Notification_Status;
  /* USER CODE BEGIN Service2_APP_Context_t */

  /* USER CODE END Service2_APP_Context_t */
  uint16_t              ConnectionHandle;
} VITAL_SIGN_SERVICE_APP_Context_t;

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
static VITAL_SIGN_SERVICE_APP_Context_t VITAL_SIGN_SERVICE_APP_Context;

uint8_t a_VITAL_SIGN_SERVICE_UpdateCharData[247];

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void VITAL_SIGN_SERVICE_Vital_data_SendNotification(void);
static void VITAL_SIGN_SERVICE_Waveform_SendNotification(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void VITAL_SIGN_SERVICE_Notification(VITAL_SIGN_SERVICE_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service2_Notification_1 */

  /* USER CODE END Service2_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service2_Notification_Service2_EvtOpcode */

    /* USER CODE END Service2_Notification_Service2_EvtOpcode */

    case VITAL_SIGN_SERVICE_DISTANCE_CONFIG_READ_EVT:
      /* USER CODE BEGIN Service2Char1_READ_EVT */

      /* USER CODE END Service2Char1_READ_EVT */
      break;

    case VITAL_SIGN_SERVICE_DISTANCE_CONFIG_WRITE_EVT:
      /* USER CODE BEGIN Service2Char1_WRITE_EVT */

      /* USER CODE END Service2Char1_WRITE_EVT */
      break;

    case VITAL_SIGN_SERVICE_VITAL_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service2Char2_NOTIFY_ENABLED_EVT */
      VITAL_SIGN_SERVICE_APP_Context.Vital_data_Notification_Status = Vital_data_NOTIFICATION_ON;
      /* USER CODE END Service2Char2_NOTIFY_ENABLED_EVT */
      break;

    case VITAL_SIGN_SERVICE_VITAL_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service2Char2_NOTIFY_DISABLED_EVT */
      VITAL_SIGN_SERVICE_APP_Context.Vital_data_Notification_Status = Vital_data_NOTIFICATION_OFF;
      /* USER CODE END Service2Char2_NOTIFY_DISABLED_EVT */
      break;

    case VITAL_SIGN_SERVICE_WAVEFORM_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service2Char3_NOTIFY_ENABLED_EVT */
      VITAL_SIGN_SERVICE_APP_Context.Waveform_Notification_Status = Waveform_NOTIFICATION_ON;
      /* USER CODE END Service2Char3_NOTIFY_ENABLED_EVT */
      break;

    case VITAL_SIGN_SERVICE_WAVEFORM_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service2Char3_NOTIFY_DISABLED_EVT */
      VITAL_SIGN_SERVICE_APP_Context.Waveform_Notification_Status = Waveform_NOTIFICATION_OFF;
      /* USER CODE END Service2Char3_NOTIFY_DISABLED_EVT */
      break;

    default:
      /* USER CODE BEGIN Service2_Notification_default */

      /* USER CODE END Service2_Notification_default */
      break;
  }
  /* USER CODE BEGIN Service2_Notification_2 */

  /* USER CODE END Service2_Notification_2 */
  return;
}

void VITAL_SIGN_SERVICE_APP_EvtRx(VITAL_SIGN_SERVICE_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service2_APP_EvtRx_1 */

  /* USER CODE END Service2_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service2_APP_EvtRx_Service2_EvtOpcode */

    /* USER CODE END Service2_APP_EvtRx_Service2_EvtOpcode */
    case VITAL_SIGN_SERVICE_CONN_HANDLE_EVT :
      /* USER CODE BEGIN Service2_APP_CONN_HANDLE_EVT */

      /* USER CODE END Service2_APP_CONN_HANDLE_EVT */
      break;

    case VITAL_SIGN_SERVICE_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN Service2_APP_DISCON_HANDLE_EVT */

      /* USER CODE END Service2_APP_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN Service2_APP_EvtRx_default */

      /* USER CODE END Service2_APP_EvtRx_default */
      break;
  }

  /* USER CODE BEGIN Service2_APP_EvtRx_2 */

  /* USER CODE END Service2_APP_EvtRx_2 */

  return;
}

void VITAL_SIGN_SERVICE_APP_Init(void)
{
  UNUSED(VITAL_SIGN_SERVICE_APP_Context);
  VITAL_SIGN_SERVICE_Init();

  /* USER CODE BEGIN Service2_APP_Init */

  /* USER CODE END Service2_APP_Init */
  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
__USED void VITAL_SIGN_SERVICE_Vital_data_SendNotification(void) /* Property Notification */
{
  VITAL_SIGN_SERVICE_APP_SendInformation_t notification_on_off = Vital_data_NOTIFICATION_OFF;
  VITAL_SIGN_SERVICE_Data_t vital_sign_service_notification_data;

  vital_sign_service_notification_data.p_Payload = (uint8_t*)a_VITAL_SIGN_SERVICE_UpdateCharData;
  vital_sign_service_notification_data.Length = 0;

  /* USER CODE BEGIN Service2Char2_NS_1 */

  /* USER CODE END Service2Char2_NS_1 */

  if (notification_on_off != Vital_data_NOTIFICATION_OFF)
  {
    VITAL_SIGN_SERVICE_UpdateValue(VITAL_SIGN_SERVICE_VITAL_DATA, &vital_sign_service_notification_data);
  }

  /* USER CODE BEGIN Service2Char2_NS_Last */

  /* USER CODE END Service2Char2_NS_Last */

  return;
}

__USED void VITAL_SIGN_SERVICE_Waveform_SendNotification(void) /* Property Notification */
{
  VITAL_SIGN_SERVICE_APP_SendInformation_t notification_on_off = Waveform_NOTIFICATION_OFF;
  VITAL_SIGN_SERVICE_Data_t vital_sign_service_notification_data;

  vital_sign_service_notification_data.p_Payload = (uint8_t*)a_VITAL_SIGN_SERVICE_UpdateCharData;
  vital_sign_service_notification_data.Length = 0;

  /* USER CODE BEGIN Service2Char3_NS_1 */

  /* USER CODE END Service2Char3_NS_1 */

  if (notification_on_off != Waveform_NOTIFICATION_OFF)
  {
    VITAL_SIGN_SERVICE_UpdateValue(VITAL_SIGN_SERVICE_WAVEFORM, &vital_sign_service_notification_data);
  }

  /* USER CODE BEGIN Service2Char3_NS_Last */

  /* USER CODE END Service2Char3_NS_Last */

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */

/* USER CODE END FD_LOCAL_FUNCTIONS */
