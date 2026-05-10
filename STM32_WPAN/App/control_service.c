/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Control_Service.c
  * @author  MCD Application Team
  * @brief   Control_Service definition.
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
#include "log_module.h"
#include "control_service.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

typedef struct{
  uint16_t  Control_serviceSvcHdle;                  /**< Control_service Service Handle */
  uint16_t  Active_ModeCharHdle;                  /**< ACTIVE_MODE Characteristic Handle */
  uint16_t  System_CommandCharHdle;                  /**< SYSTEM_COMMAND Characteristic Handle */
  uint16_t  Sensor_StatusCharHdle;                  /**< SENSOR_STATUS Characteristic Handle */
/* USER CODE BEGIN Context */
  /* Place holder for Characteristic Descriptors Handle*/

/* USER CODE END Context */
}CONTROL_SERVICE_Context_t;

/* Private defines -----------------------------------------------------------*/
#define UUID_128_SUPPORTED  1

#if (UUID_128_SUPPORTED == 1)
#define BM_UUID_LENGTH  UUID_TYPE_128
#else
#define BM_UUID_LENGTH  UUID_TYPE_16
#endif

#define BM_REQ_CHAR_SIZE    (3)

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private macros ------------------------------------------------------------*/
#define CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET         2
#define CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET              1
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
static const uint16_t SizeActive_Mode = 1;
static const uint16_t SizeSystem_Command = 1;
static const uint16_t SizeSensor_Status = 1;

static CONTROL_SERVICE_Context_t CONTROL_SERVICE_Context;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static SVCCTL_EvtAckStatus_t CONTROL_SERVICE_EventHandler(void *p_pckt);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
/* USER CODE BEGIN PFD */

/* USER CODE END PFD */

/* Private functions ----------------------------------------------------------*/

#define COPY_UUID_128(uuid_struct, uuid_15, uuid_14, uuid_13, uuid_12, uuid_11, uuid_10, uuid_9, uuid_8, uuid_7, uuid_6, uuid_5, uuid_4, uuid_3, uuid_2, uuid_1, uuid_0) \
do {\
    uuid_struct[0] = uuid_0; uuid_struct[1] = uuid_1; uuid_struct[2] = uuid_2; uuid_struct[3] = uuid_3; \
    uuid_struct[4] = uuid_4; uuid_struct[5] = uuid_5; uuid_struct[6] = uuid_6; uuid_struct[7] = uuid_7; \
    uuid_struct[8] = uuid_8; uuid_struct[9] = uuid_9; uuid_struct[10] = uuid_10; uuid_struct[11] = uuid_11; \
    uuid_struct[12] = uuid_12; uuid_struct[13] = uuid_13; uuid_struct[14] = uuid_14; uuid_struct[15] = uuid_15; \
}while(0)

/*
 The following 128bits UUIDs have been generated from the random UUID
 generator:
 0000FE40CC7A482A984A7F2ED5B3E58F: Service 128bits UUID
 0000FE418E2245419D4C21EDAE82ED19: Characteristic 128bits UUID
 0000FE428E2245419D4C21EDAE82ED19: Characteristic 128bits UUID
 0000FE438E2245419D4C21EDAE82ED19: Characteristic 128bits UUID
 */
#define COPY_CONTROL_SERVICE_UUID(uuid_struct)       COPY_UUID_128(uuid_struct,0x00,0x00,0xfe,0x40,0xcc,0x7a,0x48,0x2a,0x98,0x4a,0x7f,0x2e,0xd5,0xb3,0xe5,0x8f)
#define COPY_ACTIVE_MODE_UUID(uuid_struct)       COPY_UUID_128(uuid_struct,0x00,0x00,0xfe,0x41,0x8e,0x22,0x45,0x41,0x9d,0x4c,0x21,0xed,0xae,0x82,0xed,0x19)
#define COPY_SYSTEM_COMMAND_UUID(uuid_struct)       COPY_UUID_128(uuid_struct,0x00,0x00,0xfe,0x42,0x8e,0x22,0x45,0x41,0x9d,0x4c,0x21,0xed,0xae,0x82,0xed,0x19)
#define COPY_SENSOR_STATUS_UUID(uuid_struct)       COPY_UUID_128(uuid_struct,0x00,0x00,0xfe,0x43,0x8e,0x22,0x45,0x41,0x9d,0x4c,0x21,0xed,0xae,0x82,0xed,0x19)

/* USER CODE BEGIN PF */

/* USER CODE END PF */

/**
 * @brief  Event handler
 * @param  p_Event: Address of the buffer holding the p_Event
 * @retval Ack: Return whether the p_Event has been managed or not
 */
static SVCCTL_EvtAckStatus_t CONTROL_SERVICE_EventHandler(void *p_Event)
{
  SVCCTL_EvtAckStatus_t return_value;
  hci_event_pckt *p_event_pckt;
  evt_blecore_aci *p_blecore_evt;
  aci_gatt_attribute_modified_event_rp0 *p_attribute_modified;
  CONTROL_SERVICE_NotificationEvt_t                 notification;
  /* USER CODE BEGIN Service1_EventHandler_1 */

  /* USER CODE END Service1_EventHandler_1 */

  return_value = SVCCTL_EvtNotAck;
  p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt*)p_Event)->data);

  switch(p_event_pckt->evt)
  {
    case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
      p_blecore_evt = (evt_blecore_aci*)p_event_pckt->data;
      switch(p_blecore_evt->ecode)
      {
        case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
        {
          /* USER CODE BEGIN EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_BEGIN */

          /* USER CODE END EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_BEGIN */
          p_attribute_modified = (aci_gatt_attribute_modified_event_rp0*)p_blecore_evt->data;
          notification.ConnectionHandle         = p_attribute_modified->Connection_Handle;
          notification.AttributeHandle          = p_attribute_modified->Attr_Handle;
          notification.DataTransfered.Length    = p_attribute_modified->Attr_Data_Length;
          notification.DataTransfered.p_Payload = p_attribute_modified->Attr_Data;
          if(p_attribute_modified->Attr_Handle == (CONTROL_SERVICE_Context.Sensor_StatusCharHdle + CHARACTERISTIC_DESCRIPTOR_ATTRIBUTE_OFFSET))
          {
            return_value = SVCCTL_EvtAckFlowEnable;
            /* USER CODE BEGIN Service1_Char_3 */

            /* USER CODE END Service1_Char_3 */
            switch(p_attribute_modified->Attr_Data[0])
            {
              /* USER CODE BEGIN Service1_Char_3_attribute_modified */

              /* USER CODE END Service1_Char_3_attribute_modified */

              /* Disabled Notification management */
              case (0x00):
                /* USER CODE BEGIN Service1_Char_3_Disabled_BEGIN */

                /* USER CODE END Service1_Char_3_Disabled_BEGIN */
                notification.EvtOpcode = CONTROL_SERVICE_SENSOR_STATUS_NOTIFY_DISABLED_EVT;
                CONTROL_SERVICE_Notification(&notification);
                /* USER CODE BEGIN Service1_Char_3_Disabled_END */

                /* USER CODE END Service1_Char_3_Disabled_END */
                break;

              /* Enabled Notification management */
              case GATT_CHAR_UPDATE_SEND_NOTIFICATION:
                /* USER CODE BEGIN Service1_Char_3_COMSVC_Notification_BEGIN */

                /* USER CODE END Service1_Char_3_COMSVC_Notification_BEGIN */
                notification.EvtOpcode = CONTROL_SERVICE_SENSOR_STATUS_NOTIFY_ENABLED_EVT;
                CONTROL_SERVICE_Notification(&notification);
                /* USER CODE BEGIN Service1_Char_3_COMSVC_Notification_END */

                /* USER CODE END Service1_Char_3_COMSVC_Notification_END */
                break;

              default:
                /* USER CODE BEGIN Service1_Char_3_default */

                /* USER CODE END Service1_Char_3_default */
                break;
            }
          }

          else if(p_attribute_modified->Attr_Handle == (CONTROL_SERVICE_Context.Active_ModeCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
          {
            return_value = SVCCTL_EvtAckFlowEnable;

            notification.EvtOpcode = CONTROL_SERVICE_ACTIVE_MODE_WRITE_NO_RESP_EVT;
            /* USER CODE BEGIN Service1_Char_1_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */

            /* USER CODE END Service1_Char_1_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */
            CONTROL_SERVICE_Notification(&notification);
          }
          else if(p_attribute_modified->Attr_Handle == (CONTROL_SERVICE_Context.System_CommandCharHdle + CHARACTERISTIC_VALUE_ATTRIBUTE_OFFSET))
          {
            return_value = SVCCTL_EvtAckFlowEnable;

            notification.EvtOpcode = CONTROL_SERVICE_SYSTEM_COMMAND_WRITE_NO_RESP_EVT;
            /* USER CODE BEGIN Service1_Char_2_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */

            /* USER CODE END Service1_Char_2_ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */
            CONTROL_SERVICE_Notification(&notification);
          }

          /* USER CODE BEGIN EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_END */

          /* USER CODE END EVT_BLUE_GATT_ATTRIBUTE_MODIFIED_END */
          break;/* ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE */
        }
        case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE :
        {
          /* USER CODE BEGIN EVT_BLUE_GATT_READ_PERMIT_REQ_BEGIN */

          /* USER CODE END EVT_BLUE_GATT_READ_PERMIT_REQ_BEGIN */

          /* USER CODE BEGIN EVT_BLUE_GATT_READ_PERMIT_REQ_END */

          /* USER CODE END EVT_BLUE_GATT_READ_PERMIT_REQ_END */
          break;/* ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE */
        }
        case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE:
        {
          /* USER CODE BEGIN EVT_BLUE_GATT_WRITE_PERMIT_REQ_BEGIN */

          /* USER CODE END EVT_BLUE_GATT_WRITE_PERMIT_REQ_BEGIN */

          /* USER CODE BEGIN EVT_BLUE_GATT_WRITE_PERMIT_REQ_END */

          /* USER CODE END EVT_BLUE_GATT_WRITE_PERMIT_REQ_END */
          break;/* ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE */
        }
        case ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE:
        {
          aci_gatt_tx_pool_available_event_rp0 *p_tx_pool_available_event;
          p_tx_pool_available_event = (aci_gatt_tx_pool_available_event_rp0 *) p_blecore_evt->data;
          UNUSED(p_tx_pool_available_event);

          /* USER CODE BEGIN ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE */

          /* USER CODE END ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE */
          break;/* ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE*/
        }
        case ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE:
        {
          aci_att_exchange_mtu_resp_event_rp0 *p_exchange_mtu;
          p_exchange_mtu = (aci_att_exchange_mtu_resp_event_rp0 *)  p_blecore_evt->data;
          UNUSED(p_exchange_mtu);

          /* USER CODE BEGIN ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE */

          /* USER CODE END ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE */
          break;/* ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE */
        }
        /* USER CODE BEGIN BLECORE_EVT */

        /* USER CODE END BLECORE_EVT */
        default:
          /* USER CODE BEGIN EVT_DEFAULT */

          /* USER CODE END EVT_DEFAULT */
          break;
      }
      /* USER CODE BEGIN EVT_VENDOR */

      /* USER CODE END EVT_VENDOR */
      break; /* HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE */

      /* USER CODE BEGIN EVENT_PCKT_CASES */

      /* USER CODE END EVENT_PCKT_CASES */

    default:
      /* USER CODE BEGIN EVENT_PCKT */

      /* USER CODE END EVENT_PCKT */
      break;
  }

  /* USER CODE BEGIN Service1_EventHandler_2 */

  /* USER CODE END Service1_EventHandler_2 */

  return(return_value);
}/* end CONTROL_SERVICE_EventHandler */

/* Public functions ----------------------------------------------------------*/

/**
 * @brief  Service initialization
 * @param  None
 * @retval None
 */
void CONTROL_SERVICE_Init(void)
{
  Char_UUID_t  uuid;
  tBleStatus ret;
  uint8_t max_attr_record;

  /* USER CODE BEGIN SVCCTL_InitService1Svc_1 */

  /* USER CODE END SVCCTL_InitService1Svc_1 */

  /**
   *  Register the event handler to the BLE controller
   */
  SVCCTL_RegisterSvcHandler(CONTROL_SERVICE_EventHandler);

  /**
   * Control_Service
   *
   * Max_Attribute_Records = 1 + 2*3 + 1*no_of_char_with_notify_or_indicate_property + 1*no_of_char_with_broadcast_property
   * service_max_attribute_record = 1 for Control_Service +
   *                                2 for ACTIVE_MODE +
   *                                2 for SYSTEM_COMMAND +
   *                                2 for SENSOR_STATUS +
   *                                1 for SENSOR_STATUS configuration descriptor +
   *                              = 8
   * This value doesn't take into account number of descriptors manually added
   * In case of descriptors added, please update the max_attr_record value accordingly in the next SVCCTL_InitService User Section
   */
  max_attr_record = 8;

  /* USER CODE BEGIN SVCCTL_InitService */
  /* max_attr_record to be updated if descriptors have been added */

  /* USER CODE END SVCCTL_InitService */

  COPY_CONTROL_SERVICE_UUID(uuid.Char_UUID_128);
  ret = aci_gatt_add_service(UUID_TYPE_128,
                             (Service_UUID_t *) &uuid,
                             PRIMARY_SERVICE,
                             max_attr_record,
                             &(CONTROL_SERVICE_Context.Control_serviceSvcHdle));
  if (ret != BLE_STATUS_SUCCESS)
  {
    LOG_INFO_BLE("  Fail   : aci_gatt_add_service command: Control_Service, error code: 0x%02X\n", ret);
  }
  else
  {
    LOG_INFO_BLE("  Success: aci_gatt_add_service command: Control_serviceSvcHdle = 0x%04X\n",CONTROL_SERVICE_Context.Control_serviceSvcHdle);
  }

  /* USER CODE BEGIN SVCCTL_InitService_2 */

  /* USER CODE END SVCCTL_InitService_2 */

  /**
   * ACTIVE_MODE
   */
  COPY_ACTIVE_MODE_UUID(uuid.Char_UUID_128);
  ret = aci_gatt_add_char(CONTROL_SERVICE_Context.Control_serviceSvcHdle,
                          UUID_TYPE_128,
                          (Char_UUID_t *) &uuid,
                          SizeActive_Mode,
                          CHAR_PROP_READ | CHAR_PROP_WRITE_WITHOUT_RESP,
                          ATTR_PERMISSION_NONE,
                          GATT_NOTIFY_ATTRIBUTE_WRITE,
                          0x10,
                          CHAR_VALUE_LEN_VARIABLE,
                          &(CONTROL_SERVICE_Context.Active_ModeCharHdle));
  if (ret != BLE_STATUS_SUCCESS)
  {
    LOG_INFO_BLE("  Fail   : aci_gatt_add_char command   : ACTIVE_MODE, error code: 0x%02X\n", ret);
  }
  else
  {
    LOG_INFO_BLE("  Success: aci_gatt_add_char command   : Active_ModeCharHdle = 0x%04X\n",CONTROL_SERVICE_Context.Active_ModeCharHdle);
  }

  /* USER CODE BEGIN SVCCTL_InitService1Char1 */
  /* Place holder for Characteristic Descriptors */

  /* USER CODE END SVCCTL_InitService1Char1 */

  /**
   * SYSTEM_COMMAND
   */
  COPY_SYSTEM_COMMAND_UUID(uuid.Char_UUID_128);
  ret = aci_gatt_add_char(CONTROL_SERVICE_Context.Control_serviceSvcHdle,
                          UUID_TYPE_128,
                          (Char_UUID_t *) &uuid,
                          SizeSystem_Command,
                          CHAR_PROP_WRITE_WITHOUT_RESP,
                          ATTR_PERMISSION_NONE,
                          GATT_NOTIFY_ATTRIBUTE_WRITE,
                          0x10,
                          CHAR_VALUE_LEN_VARIABLE,
                          &(CONTROL_SERVICE_Context.System_CommandCharHdle));
  if (ret != BLE_STATUS_SUCCESS)
  {
    LOG_INFO_BLE("  Fail   : aci_gatt_add_char command   : SYSTEM_COMMAND, error code: 0x%02X\n", ret);
  }
  else
  {
    LOG_INFO_BLE("  Success: aci_gatt_add_char command   : System_CommandCharHdle = 0x%04X\n",CONTROL_SERVICE_Context.System_CommandCharHdle);
  }

  /* USER CODE BEGIN SVCCTL_InitService1Char2 */
  /* Place holder for Characteristic Descriptors */

  /* USER CODE END SVCCTL_InitService1Char2 */

  /**
   * SENSOR_STATUS
   */
  COPY_SENSOR_STATUS_UUID(uuid.Char_UUID_128);
  ret = aci_gatt_add_char(CONTROL_SERVICE_Context.Control_serviceSvcHdle,
                          UUID_TYPE_128,
                          (Char_UUID_t *) &uuid,
                          SizeSensor_Status,
                          CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                          ATTR_PERMISSION_NONE,
                          GATT_DONT_NOTIFY_EVENTS,
                          0x10,
                          CHAR_VALUE_LEN_VARIABLE,
                          &(CONTROL_SERVICE_Context.Sensor_StatusCharHdle));
  if (ret != BLE_STATUS_SUCCESS)
  {
    LOG_INFO_BLE("  Fail   : aci_gatt_add_char command   : SENSOR_STATUS, error code: 0x%02X\n", ret);
  }
  else
  {
    LOG_INFO_BLE("  Success: aci_gatt_add_char command   : Sensor_StatusCharHdle = 0x%04X\n",CONTROL_SERVICE_Context.Sensor_StatusCharHdle);
  }

  /* USER CODE BEGIN SVCCTL_InitService1Char3 */
  /* Place holder for Characteristic Descriptors */

  /* USER CODE END SVCCTL_InitService1Char3 */

  /* USER CODE BEGIN SVCCTL_InitService1Svc_2 */

  /* USER CODE END SVCCTL_InitService1Svc_2 */

  return;
}

/**
 * @brief  Characteristic update
 * @param  CharOpcode: Characteristic identifier
 * @param  pData: Structure holding data to update
 *
 */
tBleStatus CONTROL_SERVICE_UpdateValue(CONTROL_SERVICE_CharOpcode_t CharOpcode, CONTROL_SERVICE_Data_t *pData)
{
  tBleStatus ret = BLE_STATUS_INVALID_PARAMS;
  /* USER CODE BEGIN Service1_App_Update_Char_1 */

  /* USER CODE END Service1_App_Update_Char_1 */

  switch(CharOpcode)
  {
    case CONTROL_SERVICE_ACTIVE_MODE:
      ret = aci_gatt_update_char_value(CONTROL_SERVICE_Context.Control_serviceSvcHdle,
                                       CONTROL_SERVICE_Context.Active_ModeCharHdle,
                                       0, /* charValOffset */
                                       pData->Length, /* charValueLen */
                                       (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        LOG_INFO_BLE("  Fail   : aci_gatt_update_char_value ACTIVE_MODE command, error code: 0x%02X\n", ret);
      }
      else
      {
        LOG_INFO_BLE("  Success: aci_gatt_update_char_value ACTIVE_MODE command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_1 */

      /* USER CODE END Service1_Char_Value_1 */
      break;

    case CONTROL_SERVICE_SYSTEM_COMMAND:
      ret = aci_gatt_update_char_value(CONTROL_SERVICE_Context.Control_serviceSvcHdle,
                                       CONTROL_SERVICE_Context.System_CommandCharHdle,
                                       0, /* charValOffset */
                                       pData->Length, /* charValueLen */
                                       (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        LOG_INFO_BLE("  Fail   : aci_gatt_update_char_value SYSTEM_COMMAND command, error code: 0x%02X\n", ret);
      }
      else
      {
        LOG_INFO_BLE("  Success: aci_gatt_update_char_value SYSTEM_COMMAND command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_2 */

      /* USER CODE END Service1_Char_Value_2 */
      break;

    case CONTROL_SERVICE_SENSOR_STATUS:
      ret = aci_gatt_update_char_value(CONTROL_SERVICE_Context.Control_serviceSvcHdle,
                                       CONTROL_SERVICE_Context.Sensor_StatusCharHdle,
                                       0, /* charValOffset */
                                       pData->Length, /* charValueLen */
                                       (uint8_t *)pData->p_Payload);
      if (ret != BLE_STATUS_SUCCESS)
      {
        LOG_INFO_BLE("  Fail   : aci_gatt_update_char_value SENSOR_STATUS command, error code: 0x%02X\n", ret);
      }
      else
      {
        LOG_INFO_BLE("  Success: aci_gatt_update_char_value SENSOR_STATUS command\n");
      }
      /* USER CODE BEGIN Service1_Char_Value_3 */

      /* USER CODE END Service1_Char_Value_3 */
      break;

    default:
      break;
  }

  /* USER CODE BEGIN Service1_App_Update_Char_2 */

  /* USER CODE END Service1_App_Update_Char_2 */

  return ret;
}
