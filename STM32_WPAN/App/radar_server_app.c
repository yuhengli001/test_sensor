/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Radar_Server_app.c
  * @author  MCD Application Team
  * @brief   Radar_Server_app application definition.
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
#include "radar_server_app.h"
#include "radar_server.h"
#include "stm32_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "radar_sensor.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/**
  * @brief  Structure to hold Radar Control Commands 
  * (e.g., received from the phone to Start/Stop the sensor)
  **/
 typedef struct{
    uint8_t   Radar_Command_ID;  /* 0x01 = Start, 0x00 = Stop */
    uint8_t   Threshold_Value;   /* Custom sensitivity setting */
 } Radar_Control_t;

 /**
  * @brief  Structure to hold Radar Distance Data 
  * (This is what you will send TO the phone)
  **/
 typedef struct{
    uint8_t   Device_ID;         /* Identification for multiple sensors */
    uint16_t  Distance_mm;       /* Your Acconeer radar reading in millimeters */
 } Radar_Data_t;
/* USER CODE END PTD */

typedef enum
{
  A121_data_NOTIFICATION_OFF,
  A121_data_NOTIFICATION_ON,
  /* USER CODE BEGIN Service1_APP_SendInformation_t */

  /* USER CODE END Service1_APP_SendInformation_t */
  RADAR_SERVER_APP_SENDINFORMATION_LAST
} RADAR_SERVER_APP_SendInformation_t;

typedef struct
{
  RADAR_SERVER_APP_SendInformation_t     A121_data_Notification_Status;
  /* USER CODE BEGIN Service1_APP_Context_t */
  /** * @brief Context to hold the latest radar readings 
   **/
  Radar_Data_t    RadarData;    /* Distance (mm) and Device ID */
  
  /** * @brief Context to hold commands received from the phone 
   **/
  Radar_Control_t RadarControl; /* Start/Stop/Threshold commands */
  /* USER CODE END Service1_APP_Context_t */
  uint16_t              ConnectionHandle;
} RADAR_SERVER_APP_Context_t;

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
static RADAR_SERVER_APP_Context_t RADAR_SERVER_APP_Context;

uint8_t a_RADAR_SERVER_UpdateCharData[247];

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void RADAR_SERVER_A121_data_SendNotification(void);

/* USER CODE BEGIN PFP */
/** * @brief  Initialize the internal radar and BLE context.
 * This replaces the LED/Button init from the ST example.
 **/
static void Radar_Server_App_Context_Init(void);

/** * @brief  Task to process SPI data from Acconeer sensor.
 **/
static void Radar_Process_And_Send_Task(void) ;
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void RADAR_SERVER_Notification(RADAR_SERVER_NotificationEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_Notification_1 */

  /* USER CODE END Service1_Notification_1 */
  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_Notification_Service1_EvtOpcode */

    /* USER CODE END Service1_Notification_Service1_EvtOpcode */

    case RADAR_SERVER_A121_CONTROL_READ_EVT:
      /* USER CODE BEGIN Service1Char1_READ_EVT */

      /* USER CODE END Service1Char1_READ_EVT */
      break;

    case RADAR_SERVER_A121_CONTROL_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN Service1Char1_WRITE_NO_RESP_EVT */
      /* The phone sent a command! Let's check the second byte of the payload */

      if(p_Notification->DataTransfered.p_Payload[1] == 0x01)
      {
          /* COMMAND: START RADAR */
          LOG_INFO_APP("-- RADAR APP : START SCAN COMMAND RECEIVED\n");
          
          /* Update your local context so your SPI task knows to start */
          RADAR_SERVER_APP_Context.RadarControl.Radar_Command_ID = 0x01; 
          
          /* FUTURE: Add your SPI 'Enable' or 'Start' function here */
          // Acconeer_Start_Scan(); 
      }
      else if(p_Notification->DataTransfered.p_Payload[1] == 0x00)
      {
        /* COMMAND: STOP RADAR */
        LOG_INFO_APP("-- RADAR APP : STOP SCAN COMMAND RECEIVED\n");
        
        /* Update your local context */
        RADAR_SERVER_APP_Context.RadarControl.Radar_Command_ID = 0x00;
        
        /* FUTURE: Add your SPI 'Disable' or 'Power Down' function here */
        // Acconeer_Stop_Scan();
    }
      /* USER CODE END Service1Char1_WRITE_NO_RESP_EVT */
      break;

    case RADAR_SERVER_A121_DATA_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char2_NOTIFY_ENABLED_EVT */
      /* Enable the flag that allows us to push data to the phone */
      RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_data_NOTIFICATION_ON;
      
      /* Update the log for your custom board */
      LOG_INFO_APP("-- RADAR APP : DISTANCE NOTIFICATIONS ENABLED\n");
      LOG_INFO_APP(" \n\r");
      /* USER CODE END Service1Char2_NOTIFY_ENABLED_EVT */
      break;

    case RADAR_SERVER_A121_DATA_NOTIFY_DISABLED_EVT:
      /* USER CODE BEGIN Service1Char2_NOTIFY_DISABLED_EVT */
      /* Disable the flag so the BLE stack stops trying to push packets */
      RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_data_NOTIFICATION_OFF;
  
      /* Log that the phone stopped listening to the radar */
      LOG_INFO_APP("-- RADAR APP : DISTANCE NOTIFICATIONS DISABLED\n"); 
  
      /* FUTURE: Add your SPI 'Power Down' or 'Sleep' function here 
      to save battery on your custom board! */
      // Acconeer_Enter_LowPower_Mode();
      /* USER CODE END Service1Char2_NOTIFY_DISABLED_EVT */
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

void RADAR_SERVER_APP_EvtRx(RADAR_SERVER_APP_ConnHandleNotEvt_t *p_Notification)
{
  /* USER CODE BEGIN Service1_APP_EvtRx_1 */

  /* USER CODE END Service1_APP_EvtRx_1 */

  switch(p_Notification->EvtOpcode)
  {
    /* USER CODE BEGIN Service1_APP_EvtRx_Service1_EvtOpcode */

    /* USER CODE END Service1_APP_EvtRx_Service1_EvtOpcode */
    case RADAR_SERVER_CONN_HANDLE_EVT :
      /* USER CODE BEGIN Service1_APP_CONN_HANDLE_EVT */

      /* USER CODE END Service1_APP_CONN_HANDLE_EVT */
      break;

    case RADAR_SERVER_DISCON_HANDLE_EVT :
      /* USER CODE BEGIN Service1_APP_DISCON_HANDLE_EVT */
      /* Reset the Radar and BLE context now that the phone has disconnected */
      Radar_Server_App_Context_Init();
  
      /* Log the event so you can see it in your serial debugger */
      LOG_INFO_APP("-- RADAR APP : DISCONNECTED - RESETTING CONTEXT\n");
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

void RADAR_SERVER_APP_Init(void)
{
  UNUSED(RADAR_SERVER_APP_Context);
  RADAR_SERVER_Init();

  /* USER CODE BEGIN Service1_APP_Init */
  /* Register the task that pushes Radar data to the BLE stack */
  UTIL_SEQ_RegTask( 1U << CFG_TASK_SEND_RADAR_DATA_ID, UTIL_SEQ_RFU, RADAR_SERVER_A121_data_SendNotification);
  /**
   * Initialize Radar Distance Service
   */
  /* Ensure we start with notifications OFF to save power on your custom PCB */
  RADAR_SERVER_APP_Context.A121_data_Notification_Status = A121_data_NOTIFICATION_OFF;

  /* Call the custom Init function we renamed earlier */
  Radar_Server_App_Context_Init();
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
__USED void RADAR_SERVER_A121_data_SendNotification(void) /* Property Notification */
{
  RADAR_SERVER_APP_SendInformation_t notification_on_off = A121_data_NOTIFICATION_OFF;
  RADAR_SERVER_Data_t radar_server_notification_data;

  radar_server_notification_data.p_Payload = (uint8_t*)a_RADAR_SERVER_UpdateCharData;
  radar_server_notification_data.Length = 0;

  /* USER CODE BEGIN Service1Char2_NS_1 */
  uint16_t current_distance = 0;
  uint8_t object_count = 0;

  if (RADAR_SERVER_APP_Context.A121_data_Notification_Status == A121_data_NOTIFICATION_ON)
  {
      /* Trigger a live physical measurement from the Acconeer sensor */
      if(Radar_Sensor_Get_Next(&current_distance, &object_count)) {
          RADAR_SERVER_APP_Context.RadarData.Distance_mm = current_distance;
          LOG_INFO_APP("-- RADAR APP : SENDING DIST: %d mm (Targets: %d)\n", current_distance, object_count);
      } else {
          /* Fallback to last known good distance if sensor read fails */
          current_distance = RADAR_SERVER_APP_Context.RadarData.Distance_mm;
          LOG_INFO_APP("-- RADAR APP : SENSOR SPI READ FAILED! Sending old %d mm\n", current_distance);
      }
      notification_on_off = A121_data_NOTIFICATION_ON;
  }
  else
  {
      notification_on_off = A121_data_NOTIFICATION_OFF;
  }

  /* 2. Format the new 4-byte BLE payload! */
  a_RADAR_SERVER_UpdateCharData[0] = (uint8_t)(current_distance >> 8);   /* Dist High Byte */
  a_RADAR_SERVER_UpdateCharData[1] = (uint8_t)(current_distance & 0xFF); /* Dist Low Byte */
  a_RADAR_SERVER_UpdateCharData[2] = object_count;                       /* Object Count */
  a_RADAR_SERVER_UpdateCharData[3] = 0x00;                               /* Reserved Byte */

  /* 3. Update the data length to 4 bytes */
  radar_server_notification_data.Length = 4;
  /* USER CODE END Service1Char2_NS_1 */

  if (notification_on_off != A121_data_NOTIFICATION_OFF)
  {
    RADAR_SERVER_UpdateValue(RADAR_SERVER_A121_DATA, &radar_server_notification_data);
  }

  /* USER CODE BEGIN Service1Char2_NS_Last */

  /* USER CODE END Service1Char2_NS_Last */

  return;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
/**
 * @brief  Initialize the Radar application context.
 * Sets default values for your SPI sensor data and BLE communication.
 */
static void Radar_Server_App_Context_Init(void)
{
  /* Initialize the physical Acconeer Sensor! */
  if(Radar_Sensor_Init()) {
      LOG_INFO_APP("-- RADAR APP : SENSOR INITIALIZED SUCCESSFULLY\n");
  } else {
      LOG_INFO_APP("-- RADAR APP : SENSOR INITIALIZATION FAILED!\n");
  }

  /* 1. Initialize your Radar Data structure */
  RADAR_SERVER_APP_Context.RadarData.Device_ID = 0x01;      /* Set your primary Sensor ID */
  RADAR_SERVER_APP_Context.RadarData.Distance_mm = 0xFFFF;       /* Waiting for radar data */

  /* 2. Initialize your Radar Control (Commands from phone) */
  RADAR_SERVER_APP_Context.RadarControl.Radar_Command_ID = 0x00; /* Default: Stopped */
  RADAR_SERVER_APP_Context.RadarControl.Threshold_Value = 50;    /* Set a default sensitivity */

  /* 3. Log that the Radar System is ready */
  LOG_INFO_APP("--RADAR APP : CONTEXT INITIALIZED\n");

  return;
}
/* USER CODE END FD_LOCAL_FUNCTIONS */
