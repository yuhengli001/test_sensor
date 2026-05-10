/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Vibration_Service_app.c
  * @author  MCD Application Team
  * @brief   Vibration_Service_app application definition.
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
#include "vibration_service_app.h"
#include "vibration_service.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef enum
{
  Vibration_data_NOTIFICATION_OFF,
  Vibration_data_NOTIFICATION_ON,
  Spectrum_array_NOTIFICATION_OFF,
  Spectrum_array_NOTIFICATION_ON,
  /* USER CODE BEGIN Service4_APP_SendInformation_t */

  /* USER CODE END Service4_APP_SendInformation_t */
  VIBRATION_SERVICE_APP_SENDINFORMATION_LAST
} VIBRATION_SERVICE_APP_SendInformation_t;

typedef struct
{
  VIBRATION_SERVICE_APP_SendInformation_t     Vibration_data_Notification_Status;
  VIBRATION_SERVICE_APP_SendInformation_t     Spectrum_array_Notification_Status;
  /* USER CODE BEGIN Service4_APP_Context_t */

  /* USER CODE END Service4_APP_Context_t */
  uint16_t              ConnectionHandle;
} VIBRATION_SERVICE_APP_Context_t;

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
static VIBRATION_SERVICE_APP_Context_t VIBRATION_SERVICE_APP_Context;

uint8_t a_VIBRATION_SERVICE_UpdateCharData[247];

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void VIBRATION_SERVICE_Vibration_data_SendNotification(void);
static void VIBRATION_SERVICE_Spectrum_array_SendNotification(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void VIBRATION_SERVICE_Notification(VIBRATION_SERVICE_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service4_Notification_1 */

  /* USER CODE END Service4_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service4_Notification_Service4_EvtOpcode */

    /* USER CODE END Service4_Notification_Service4_EvtOpcode */

    case VIBRATION_SERVICE_VIBRATION_CONFIG_READ_EVT:
      /* USER CODE BEGIN Service4Char1_READ_EVT */

      /* USER CODE END Service4Char1_READ_EVT */
      break;

    case VIBRATION_SERVICE_VIBRATION_CONFIG_WRITE_EVT:
      /* USER CODE BEGIN Service4Char1_WRITE_EVT */

      /* USER CODE END Service4Char1_WRITE_EVT */
      break;

    case VIBRATION_SERVICE_VIBRATION_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service4Char2_NOTIFY_ENABLED_EVT */

      /* USER CODE END Service4Char2_NOTIFY_ENABLED_EVT */
      break;

    case VIBRATION_SERVICE_VIBRATION_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service4Char2_NOTIFY_DISABLED_EVT */

      /* USER CODE END Service4Char2_NOTIFY_DISABLED_EVT */
      break;

    case VIBRATION_SERVICE_SPECTRUM_ARRAY_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service4Char3_NOTIFY_ENABLED_EVT */

      /* USER CODE END Service4Char3_NOTIFY_ENABLED_EVT */
      break;

    case VIBRATION_SERVICE_SPECTRUM_ARRAY_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service4Char3_NOTIFY_DISABLED_EVT */

      /* USER CODE END Service4Char3_NOTIFY_DISABLED_EVT */
      break;

    default:
      /* USER CODE BEGIN Service4_Notification_default */

      /* USER CODE END Service4_Notification_default */
      break;
  }
  /* USER CODE BEGIN Service4_Notification_2 */

  /* USER CODE END Service4_Notification_2 */
  return;
}

void VIBRATION_SERVICE_APP_EvtRx(VIBRATION_SERVICE_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service4_APP_EvtRx_1 */

  /* USER CODE END Service4_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service4_APP_EvtRx_Service4_EvtOpcode */

    /* USER CODE END Service4_APP_EvtRx_Service4_EvtOpcode */
    case VIBRATION_SERVICE_CONN_HANDLE_EVT :
      /* USER CODE BEGIN Service4_APP_CONN_HANDLE_EVT */

      /* USER CODE END Service4_APP_CONN_HANDLE_EVT */
      break;

    case VIBRATION_SERVICE_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN Service4_APP_DISCON_HANDLE_EVT */

      /* USER CODE END Service4_APP_DISCON_HANDLE_EVT */
      break;

    default:
      /* USER CODE BEGIN Service4_APP_EvtRx_default */

      /* USER CODE END Service4_APP_EvtRx_default */
      break;
  }

  /* USER CODE BEGIN Service4_APP_EvtRx_2 */

  /* USER CODE END Service4_APP_EvtRx_2 */

  return;
}

void VIBRATION_SERVICE_APP_Init(void)
{
  UNUSED(VIBRATION_SERVICE_APP_Context);
  VIBRATION_SERVICE_Init();

  /* USER CODE BEGIN Service4_APP_Init */

  /* USER CODE END Service4_APP_Init */
  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/*************************************************************
 *
 * LOCAL FUNCTIONS
 *
 *************************************************************/
__USED void VIBRATION_SERVICE_Vibration_data_SendNotification(void) /* Property Notification */
{
  VIBRATION_SERVICE_APP_SendInformation_t notification_on_off = Vibration_data_NOTIFICATION_OFF;
  VIBRATION_SERVICE_Data_t vibration_service_notification_data;

  vibration_service_notification_data.p_Payload = (uint8_t*)a_VIBRATION_SERVICE_UpdateCharData;
  vibration_service_notification_data.Length = 0;

  /* USER CODE BEGIN Service4Char2_NS_1 */

  /* USER CODE END Service4Char2_NS_1 */

  if (notification_on_off != Vibration_data_NOTIFICATION_OFF)
  {
    VIBRATION_SERVICE_UpdateValue(VIBRATION_SERVICE_VIBRATION_DATA, &vibration_service_notification_data);
  }

  /* USER CODE BEGIN Service4Char2_NS_Last */

  /* USER CODE END Service4Char2_NS_Last */

  return;
}

__USED void VIBRATION_SERVICE_Spectrum_array_SendNotification(void) /* Property Notification */
{
  VIBRATION_SERVICE_APP_SendInformation_t notification_on_off = Spectrum_array_NOTIFICATION_OFF;
  VIBRATION_SERVICE_Data_t vibration_service_notification_data;

  vibration_service_notification_data.p_Payload = (uint8_t*)a_VIBRATION_SERVICE_UpdateCharData;
  vibration_service_notification_data.Length = 0;

  /* USER CODE BEGIN Service4Char3_NS_1 */

  /* USER CODE END Service4Char3_NS_1 */

  if (notification_on_off != Spectrum_array_NOTIFICATION_OFF)
  {
    VIBRATION_SERVICE_UpdateValue(VIBRATION_SERVICE_SPECTRUM_ARRAY, &vibration_service_notification_data);
  }

  /* USER CODE BEGIN Service4Char3_NS_Last */

  /* USER CODE END Service4Char3_NS_Last */

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */

/* USER CODE END FD_LOCAL_FUNCTIONS */
