/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Radar_Server_app.c
  * @author  MCD Application Team
  * @brief   Radar_Server_app application definition.
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
#include "radar_server_app.h"
#include "radar_server.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "radar_sensor.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef struct{
    uint8_t   Radar_Command_ID;
    uint8_t   Threshold_Value;
} Radar_Control_t;

typedef struct{
    uint8_t   Device_ID;
    uint16_t  Distance_mm;
} Radar_Data_t;

typedef enum
{
  A121_data_NOTIFICATION_OFF,
  A121_data_NOTIFICATION_ON,
} RADAR_SERVER_APP_SendInformation_t;

typedef struct
{
  RADAR_SERVER_APP_SendInformation_t     A121_data_Notification_Status;
  Radar_Data_t    RadarData;
  Radar_Control_t RadarControl;
  uint16_t        ConnectionHandle;
} RADAR_SERVER_APP_Context_t;

/* Private variables ---------------------------------------------------------*/
static RADAR_SERVER_APP_Context_t RADAR_SERVER_APP_Context;
uint8_t a_RADAR_SERVER_UpdateCharData[247];

/* Private function prototypes -----------------------------------------------*/
static void RADAR_SERVER_A121_data_SendNotification(void);
static void Radar_Server_App_Context_Init(void);

/* Functions Definition ------------------------------------------------------*/
void RADAR_SERVER_Notification(RADAR_SERVER_NotificationEvt_t *p_Notification)
{
  switch(p_Notification->EvtOpcode)
  {
    case RADAR_SERVER_A121_CONTROL_WRITE_NO_RESP_EVT:
      LOG_INFO_APP("-- RADAR APP : CMD RECEIVED. LEN: %d, DATA: %02X %02X\n", 
                   p_Notification->DataTransfered.Length,
                   p_Notification->DataTransfered.p_Payload[0],
                   p_Notification->DataTransfered.p_Payload[1]);

      if(p_Notification->DataTransfered.p_Payload[0] == 0x02) // Action Command
      {
          if (p_Notification->DataTransfered.p_Payload[1] == 0x01) {
              LOG_INFO_APP("-- RADAR APP : START COMMAND RECEIVED\n");
              Radar_Sensor_Start();
          } else {
              LOG_INFO_APP("-- RADAR APP : STOP COMMAND RECEIVED\n");
              Radar_Sensor_Stop();
          }
      }
      else if (p_Notification->DataTransfered.p_Payload[0] == 0x03 && 
               p_Notification->DataTransfered.Length >= 6) // Param Update
      {
          uint8_t param_id = p_Notification->DataTransfered.p_Payload[1];
          union {
              uint8_t  b[4];
              uint32_t u32;
          } converter;
          converter.b[0] = p_Notification->DataTransfered.p_Payload[2];
          converter.b[1] = p_Notification->DataTransfered.p_Payload[3];
          converter.b[2] = p_Notification->DataTransfered.p_Payload[4];
          converter.b[3] = p_Notification->DataTransfered.p_Payload[5];

          LOG_INFO_APP("-- RADAR APP : PARAM UPDATE. ID: %02X\n", param_id);
          Radar_Sensor_UpdateParam((radar_param_id_t)param_id, &converter.u32);
      }
      break;

    case RADAR_SERVER_A121_DATA_NOTIFY_ENABLED_EVT:
      RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_data_NOTIFICATION_ON;
      LOG_INFO_APP("-- RADAR APP : DISTANCE NOTIFICATIONS ENABLED\n");
      break;

    case RADAR_SERVER_A121_DATA_NOTIFY_DISABLED_EVT:
      RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_data_NOTIFICATION_OFF;
      LOG_INFO_APP("-- RADAR APP : DISTANCE NOTIFICATIONS DISABLED\n");
      break;

    default:
      break;
  }
}

void RADAR_SERVER_APP_EvtRx(RADAR_SERVER_APP_ConnHandleNotEvt_t *p_Notification)
{
  switch(p_Notification->EvtOpcode)
  {
    case RADAR_SERVER_CONN_HANDLE_EVT :
      LOG_INFO_APP("-- RADAR APP : PHONE CONNECTED\n");
      break;

    case RADAR_SERVER_DISCON_HANDLE_EVT :
      Radar_Sensor_Stop();
      LOG_INFO_APP("-- RADAR APP : PHONE DISCONNECTED - SENSOR STOPPED\n");
      Radar_Server_App_Context_Init();
      break;

    default:
      break;
  }
}

void RADAR_SERVER_APP_Init(void)
{
  RADAR_SERVER_Init();
  UTIL_SEQ_RegTask( 1U << CFG_TASK_SEND_RADAR_DATA_ID, UTIL_SEQ_RFU, RADAR_SERVER_A121_data_SendNotification);
  RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_data_NOTIFICATION_OFF;
  Radar_Server_App_Context_Init();
}

__USED void RADAR_SERVER_A121_data_SendNotification(void)
{
  RADAR_SERVER_APP_SendInformation_t notification_on_off = RADAR_SERVER_APP_Context.A121_data_Notification_Status;
  RADAR_SERVER_Data_t radar_server_notification_data;
  radar_server_notification_data.p_Payload = (uint8_t*)a_RADAR_SERVER_UpdateCharData;

  float distances[5];
  float strengths[5];
  uint8_t num_targets = 0;

  /* Trigger a real-time measurement if notifications are enabled */
  if (notification_on_off == A121_data_NOTIFICATION_ON)
  {
      if(Radar_Sensor_Get_Next_Results(distances, strengths, &num_targets)) 
      {
          /* Format structured BLE payload: [Type:0x01][Num][Dist0][Str0]... */
          a_RADAR_SERVER_UpdateCharData[0] = 0x01; 
          a_RADAR_SERVER_UpdateCharData[1] = num_targets;
          
          uint8_t offset = 2;
          for (uint8_t i = 0; i < num_targets && i < 2; i++) {
              memcpy(&a_RADAR_SERVER_UpdateCharData[offset], &distances[i], 4);
              offset += 4;
              memcpy(&a_RADAR_SERVER_UpdateCharData[offset], &strengths[i], 4);
              offset += 4;
          }
          radar_server_notification_data.Length = offset;
          RADAR_SERVER_UpdateValue(RADAR_SERVER_A121_DATA, &radar_server_notification_data);

          if (num_targets > 0) {
              LOG_INFO_APP("Radar: %d targets, nearest: %.4f m\n", num_targets, distances[0]);
          }
      }
  }
}

static void Radar_Server_App_Context_Init(void)
{
  if(Radar_Sensor_PreInit()) {
      LOG_INFO_APP("-- RADAR APP : SOFTWARE RESOURCES PRE-INIT SUCCESSFUL\n");
  }
  RADAR_SERVER_APP_Context.RadarData.Device_ID = 0x01;
  RADAR_SERVER_APP_Context.RadarData.Distance_mm = 0xFFFF;
}
