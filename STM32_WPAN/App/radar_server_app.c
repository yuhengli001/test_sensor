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
				// Action command: 0x02 0x01 = start, 0x02 0x00 = stop
				if (p_Notification->DataTransfered.p_Payload[1] == 0x01)
				{
					LOG_INFO_APP("-- RADAR APP : start command received\n");
					Radar_Sensor_Start();
				}
				else
				{
					LOG_INFO_APP("-- RADAR APP : stop command received\n");
					Radar_Sensor_Stop();
				}
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

	float   distances[5];
	float   strengths[5];
	uint8_t num_targets = 0;

	if (!Radar_Sensor_Get_Next_Results(distances, strengths, &num_targets))
	{
		return;
	}

	// Build BLE payload: [0x01][num_targets][dist0: 4B][str0: 4B]...
	RADAR_SERVER_Data_t notification_data;
	notification_data.p_Payload = (uint8_t *)a_RADAR_SERVER_UpdateCharData;

	a_RADAR_SERVER_UpdateCharData[0] = 0x01;
	a_RADAR_SERVER_UpdateCharData[1] = num_targets;

	uint8_t offset = 2;
	for (uint8_t i = 0; i < num_targets && i < 2; i++)
	{
		memcpy(&a_RADAR_SERVER_UpdateCharData[offset], &distances[i], 4);
		offset += 4;
		memcpy(&a_RADAR_SERVER_UpdateCharData[offset], &strengths[i], 4);
		offset += 4;
	}

	notification_data.Length = offset;
	RADAR_SERVER_UpdateValue(RADAR_SERVER_A121_DATA, &notification_data);

	if (num_targets > 0)
	{
		LOG_INFO_APP("Radar: %d target(s), nearest=%.4f m\n", num_targets, distances[0]);
	}
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
