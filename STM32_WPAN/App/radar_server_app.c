/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Radar_Server_app.c
  * @brief   Radar BLE server application — handles BLE commands and data notifications.
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

/* USER CODE BEGIN Includes */
#include "radar_sensor.h"
#include "fall_detector.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
	uint8_t Radar_Command_ID;
	uint8_t Threshold_Value;
} Radar_Control_t;

typedef struct
{
	uint8_t  Device_ID;
	uint16_t Distance_mm;
} Radar_Data_t;

typedef enum
{
	A121_DATA_NOTIFICATION_OFF,
	A121_DATA_NOTIFICATION_ON,
} RADAR_SERVER_APP_SendInformation_t;

typedef struct
{
	RADAR_SERVER_APP_SendInformation_t A121_data_Notification_Status;
	Radar_Data_t                       RadarData;
	Radar_Control_t                    RadarControl;
	uint16_t                           ConnectionHandle;
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
	switch (p_Notification->EvtOpcode)
	{
		case RADAR_SERVER_A121_CONTROL_WRITE_NO_RESP_EVT:
		{
			LOG_INFO_APP("-- RADAR APP : CMD received. len=%d, data=%02X %02X\n",
			             p_Notification->DataTransfered.Length,
			             p_Notification->DataTransfered.p_Payload[0],
			             p_Notification->DataTransfered.p_Payload[1]);

			uint8_t cmd = p_Notification->DataTransfered.p_Payload[0];

			if (cmd == 0x02)
			{
				// 0x02 start/stop command is reserved for the distance detector.
				// The presence detector (phase mode) runs continuously in the main loop
				// and cannot be started or stopped via this command.
				// Silently ignore to prevent a hardware conflict with the active sensor.
				LOG_INFO_APP("-- RADAR APP : 0x02 command ignored (presence detector active)\n");
			}
			else if (cmd == 0x03 && p_Notification->DataTransfered.Length >= 6)
			{
				// Parameter update command: 0x03 [param_id] [value: 4 bytes LE]
				uint8_t param_id = p_Notification->DataTransfered.p_Payload[1];

				union
				{
					uint8_t  b[4];
					uint32_t u32;
				} converter;

				converter.b[0] = p_Notification->DataTransfered.p_Payload[2];
				converter.b[1] = p_Notification->DataTransfered.p_Payload[3];
				converter.b[2] = p_Notification->DataTransfered.p_Payload[4];
				converter.b[3] = p_Notification->DataTransfered.p_Payload[5];

				LOG_INFO_APP("-- RADAR APP : param update. ID=0x%02X\n", param_id);
				Radar_Sensor_UpdateParam((radar_param_id_t)param_id, &converter.u32);
			}
			break;
		}

		case RADAR_SERVER_A121_DATA_NOTIFY_ENABLED_EVT:
			RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_DATA_NOTIFICATION_ON;
			LOG_INFO_APP("-- RADAR APP : notifications enabled\n");
			break;

		case RADAR_SERVER_A121_DATA_NOTIFY_DISABLED_EVT:
			RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_DATA_NOTIFICATION_OFF;
			LOG_INFO_APP("-- RADAR APP : notifications disabled\n");
			break;

		default:
			break;
	}
}


void RADAR_SERVER_APP_EvtRx(RADAR_SERVER_APP_ConnHandleNotEvt_t *p_Notification)
{
	switch (p_Notification->EvtOpcode)
	{
		case RADAR_SERVER_CONN_HANDLE_EVT:
			LOG_INFO_APP("-- RADAR APP : phone connected\n");
			break;

		case RADAR_SERVER_DISCON_HANDLE_EVT:
			Radar_Sensor_Stop();
			LOG_INFO_APP("-- RADAR APP : phone disconnected, sensor stopped\n");
			Radar_Server_App_Context_Init();
			break;

		default:
			break;
	}
}


void RADAR_SERVER_APP_Init(void)
{
	RADAR_SERVER_Init();
	UTIL_SEQ_RegTask(1U << CFG_TASK_SEND_RADAR_DATA_ID, UTIL_SEQ_RFU, RADAR_SERVER_A121_data_SendNotification);
	RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_DATA_NOTIFICATION_OFF;
	Radar_Server_App_Context_Init();
}


__USED void RADAR_SERVER_A121_data_SendNotification(void)
{
	if (RADAR_SERVER_APP_Context.A121_data_Notification_Status != A121_DATA_NOTIFICATION_ON)
	{
		return;
	}

	if (!g_presence_valid)
	{
		return;
	}

	// BLE payload format (7 bytes):
	//   [0]    0x01 — packet type: presence detector data
	//   [1]    0x01 — num_targets (presence detector tracks one subject)
	//   [2..5] subject distance in metres, IEEE-754 float, little-endian
	//   [6]    fall status: 1=normal, 2=impact, 3=suspected, 4=ALARM
	RADAR_SERVER_Data_t notification_data;
	notification_data.p_Payload = (uint8_t *)a_RADAR_SERVER_UpdateCharData;

	a_RADAR_SERVER_UpdateCharData[0] = 0x01;
	a_RADAR_SERVER_UpdateCharData[1] = 0x01;
	memcpy(&a_RADAR_SERVER_UpdateCharData[2], &g_presence_dist, 4);
	a_RADAR_SERVER_UpdateCharData[6] = (uint8_t)g_fall_status;

	notification_data.Length = 7;
	RADAR_SERVER_UpdateValue(RADAR_SERVER_A121_DATA, &notification_data);

	LOG_INFO_APP("BLE TX: dist=%.3f m | fall=%d (%s)\n",
	             g_presence_dist,
	             (int)g_fall_status,
	             g_fall_status == FALL_STATUS_ALARM     ? "ALARM"     :
	             g_fall_status == FALL_STATUS_SUSPECTED ? "SUSPECTED" :
	             g_fall_status == FALL_STATUS_IMPACT    ? "IMPACT"    : "normal");
}


static void Radar_Server_App_Context_Init(void)
{
	if (Radar_Sensor_PreInit())
	{
		LOG_INFO_APP("-- RADAR APP : software pre-init successful\n");
	}

	RADAR_SERVER_APP_Context.RadarData.Device_ID    = 0x01;
	RADAR_SERVER_APP_Context.RadarData.Distance_mm  = 0xFFFF;
}
