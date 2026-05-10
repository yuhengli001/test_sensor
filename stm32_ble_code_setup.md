# STM32 WPAN BLE Tutorial to Radar Mapping

This document breaks down the official STMicroelectronics BLE P2P Server tutorial step-by-step. For each step, it explains what the ST tutorial is doing and how we adapt that exact concept for our multi-service Radar application.

---

## Step 1: The Background Sequencer Task ID
**File:** `Core/Inc/app_conf.h`

### What the ST Tutorial Does:
In the ST tutorial, they want to send a BLE notification when a physical button on the Nucleo board is pressed. However, button presses trigger hardware interrupts. You **cannot** safely run heavy BLE functions inside a hardware interrupt. 

To solve this, ST uses the **Sequencer**. They go to the `CFG_Task_Id_t` enumeration in `app_conf.h` and add a new ID:
```c
/* USER CODE BEGIN CFG_Task_Id_t */
CFG_TASK_SW1_BUTTON_PUSHED_ID,
/* USER CODE END CFG_Task_Id_t */
```
This simply registers a "slot" on the sequencer's To-Do list. 

### How we adapt it for our Radar:
Radar measurements (sending pulses, calculating FFTs, etc.) take time. If we ran the radar loop directly inside the BLE connection loop, the STM32 would be so busy doing math that it would drop the BLE connection.

We adapt ST's button logic to our radar by giving our Radar its own slot on the sequencer's To-Do list. 
Around line 368 in your `app_conf.h`, we already have this:
```c
/* USER CODE BEGIN CFG_Task_Id_t */
CFG_TASK_ADV_CANCEL_ID,
CFG_TASK_SEND_RADAR_DATA_ID,
/* USER CODE END CFG_Task_Id_t */
```
**Conclusion of Step 1:** By defining `CFG_TASK_SEND_RADAR_DATA_ID`, we have officially reserved a safe space for the Radar to do its math in the background without breaking the Bluetooth connection.

### A Note on the ST Tutorial's `app_conf.h` Macros
You noticed that the ST tutorial also adds these lines:
```c
#define CFG_LED_SUPPORTED         1
#define CFG_BUTTON_SUPPORTED      1
#define PUSH_BUTTON_SW1_EXTI_IRQHandler     EXTI4_IRQHandler
```
**What the ST tutorial does:** These macros map the physical plastic buttons and LEDs on the ST Nucleo board to specific hardware interrupt handlers (EXTI). 

**How we adapt it:** We skip this completely! Our Radar application doesn't rely on you physically pressing a button on the board. Instead, our "button" is a virtual one: the user pressing "Start" on the mobile phone app via Bluetooth. Therefore, we do not need to configure hardware interrupts for buttons.

### Are these two tasks enough for 4 services?
Yes! You might wonder: *"Since we have Vital Sign, Fall Detection, and Vibration, shouldn't we have a task for each?"* 

It is actually much safer to use a **single** radar task (`CFG_TASK_SEND_RADAR_DATA_ID`). If you had three separate tasks trying to run at the same time, they would fight over the SPI wires connecting the STM32 to the Acconeer sensor, causing a crash. By using one task, we will create a simple "Switch" inside it that looks at the `current_radar_mode` and gracefully runs the correct math.

### What exactly is the Sequencer?
The Sequencer (`UTIL_SEQ`) is a lightweight "To-Do list" manager created by ST. Bluetooth Low Energy (BLE) has extremely strict microsecond timing requirements. If your code interrupts the BLE radio at the wrong time, the phone will immediately disconnect. Instead of using a heavy RTOS (like FreeRTOS), ST provides the Sequencer. You give the Sequencer your heavy radar math function. When the CPU is safe and not doing critical BLE radio work, the Sequencer says, "Okay, it's safe now, run your math!"

### What is `CFG_TASK_ADV_CANCEL_ID`?
"ADV" stands for Advertising. When your STM32 is not connected to a phone, it sends out "Advertising" packets (shouting "I'm here!"). To save battery, you usually don't want it to shout forever. This task is used by a built-in timer. If no phone connects after a certain amount of time, a timer triggers this task, and the sequencer gracefully shuts off the BLE radio advertising to save power. You don't need to worry about it, CubeMX handles it automatically!

---

## Step 2: Hardware Initialization 
**File in ST Tutorial:** `app_entry.c`

### What the ST Tutorial Does:
In the ST tutorial (and in your actual `app_entry.c` file!), you can see this code inside `MX_APPE_Init()`:
```c
/* USER CODE BEGIN APPE_Init_1 */
  Led_Init();
  Button_Init();
/* USER CODE END APPE_Init_1 */
```
And further down, it defines `Led_Init()` to turn on `LED_GREEN`, and an EXTI Callback that triggers `APP_BLE_Key_Button1_Action()` when the physical button is pressed.

### How we adapt it for our Radar:
This code was likely placed here by the default ST Nucleo template. Here is how we should adapt it:

1. **The Button (`Button_Init`):** We don't need the physical button because our phone acts as the button over BLE. You can safely ignore the `HAL_GPIO_EXTI_Callback`.
2. **The LED (`Led_Init`):** It is actually a **great idea** to keep this! We can use the LED for debugging. Later on, when the phone sends the "START" command, we can write code to turn the Green LED on, and when it sends "STOP", we turn the LED off. 

So for Step 2, you are already perfectly set up. Your hardware (the LEDs) are initialized and ready to be used as status indicators for the radar!

---

## Step 3: Hardware Interrupts to BLE 
**File in ST Tutorial:** `app_ble.c`

### What the ST Tutorial Does:
The tutorial asks you to add this block of code:
```c
/* USER CODE BEGIN FD_SPECIFIC_FUNCTIONS */
void APP_BLE_Key_Button1_Action(void)
{
  P2PS_APP_SW1_Button_Action();
}
/* USER CODE END FD_SPECIFIC_FUNCTIONS */
```
When you physically push the plastic button on the Nucleo board, the STM32 generates a hardware interrupt. This interrupt calls `APP_BLE_Key_Button1_Action()`, which then triggers the BLE logic to send a notification to the phone.

### How we adapt it for our Radar:
We **skip this completely!** 

In the ST tutorial, the flow of data is: **Hardware Button -> BLE Stack -> Phone**. 
In our Radar app, the flow of commands is the exact opposite! It goes: **Phone -> BLE Stack -> Hardware Radar**. 

Because we don't have physical buttons commanding our system, we don't need to link any hardware interrupts in `app_ble.c`. You can leave your `app_ble.c` exactly as it was auto-generated.

### A Note on `app_ble.h` and `custom_app.h`
The tutorial also asks you to go into `app_ble.h` and `custom_app.h` (or `p2p_server_app.h`) to declare functions like `APP_BLE_Key_Button1_Action(void);` and `P2PS_APP_SW1_Button_Action(void);`.

Why do they do this? Because their hardware button (in `app_entry.c`) talks to the BLE stack (`app_ble.c`), which then talks to the application layer (`custom_app.c`). They need headers to link these files together. 

Since we didn't create these functions (because we have no button sending commands inward), we **skip modifying both of these header files**!

---

## Step 4: The Application Logic
**File in ST Tutorial:** `custom_app.c` (or `p2p_server_app.c`)
**File in our Radar App:** `control_service_app.c`

This is the most important step in the entire tutorial. This is where the Bluetooth data is actually read, and where the STM32 decides what to do with it. Let's break down the ST tutorial into parts and map it to our project!

### Crash Course: GATT Properties (Who to Who?)
Before we write code, we must understand the direction of data flow. In BLE, the perspective is **always from the Phone (the Client)** talking to the **STM32 (the Server)**:

*   **WRITE:** The Phone sends data **to** the STM32. (Phone -> STM32). *Example: Phone sends the START command.*
*   **READ:** The Phone pulls data **from** the STM32. (Phone <- STM32). *Example: Phone checks what the current radar mode is.*
*   **NOTIFY:** The STM32 pushes data **to** the Phone automatically, without the phone having to ask. (STM32 -> Phone). *Example: STM32 streams live Vital Signs data 10 times a second.*

### Part 1: State Variables
**Why did CubeMX auto-generate `Sensor_status_Notification_Status` but not variables for Active Mode or System Command?**
By BLE law, a device is not allowed to send a **NOTIFY** packet unless the Phone explicitly permits it first. Therefore, CubeMX *automatically* generates a tracking variable (a "flag") for every characteristic that has the NOTIFY property. 

However, **WRITE** characteristics (like Active Mode and System Command) do not have this strict BLE rule. They are just events. CubeMX passes the event to you, and leaves it up to you as the developer to create your own variables to save the data.

**ST Tutorial:** They add `SW1_Status` to remember if the button was pushed.
```c
/* USER CODE BEGIN CUSTOM_APP_Context_t */
  uint8_t               SW1_Status;
/* USER CODE END CUSTOM_APP_Context_t */
```

**Our Radar:** Because `Active_Mode` is just a Write event, we must create our own variables to remember what the phone sent. We added `current_radar_mode` (to remember Vital vs Fall vs Vibe) and `is_radar_running` (to remember if it's currently turned on). 
```c
/* USER CODE BEGIN PV */
uint8_t current_radar_mode = 0; /* 0: None, 1: Vital, 2: Fall, 3: Vibration */
uint8_t is_radar_running = 0;   /* 0: Stopped, 1: Running */
/* USER CODE END PV */
```
*(Note: The ST tutorial puts their variable inside the `Context_t` struct block. In our code, because `control_service_app.c` generated `Service1_APP_Context_t`, we could put it there too! However, putting it in `/* USER CODE BEGIN PV */` (Private Variables) is often easier to read and access across different functions in the file. Both ways are perfectly correct in C!)*

### Part 2: Catching Bluetooth Writes
**ST Tutorial:** Under `CUSTOM_STM_LED_C_WRITE_NO_RESP_EVT`, they read the payload. If `0x01`, turn on LED. If `0x00`, turn off.
```c
    case CUSTOM_STM_LED_C_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN CUSTOM_STM_LED_C_WRITE_NO_RESP_EVT */
      if(pNotification->DataTransfered.pPayload[1] == 0x01) {
        BSP_LED_On(LED_BLUE);
      }
      /* ... */
```

**Our Radar:** We have TWO writeable characteristics. 
For **System Command**, if it is `0x01` (START), we set `is_radar_running = 1` and **Wake up the Sequencer** (`UTIL_SEQ_SetTask`).
```c
    case CONTROL_SERVICE_SYSTEM_COMMAND_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN Service1Char2_WRITE_NO_RESP_EVT */
      if (p_Notification->DataTransfered.Length == 1) {
          uint8_t cmd = p_Notification->DataTransfered.p_Payload[0];
          if (cmd == 0x01) /* START */ {
              is_radar_running = 1;
              UTIL_SEQ_SetTask(1<<CFG_TASK_SEND_RADAR_DATA_ID, CFG_SEQ_PRIO_0);
          }
      }
      /* USER CODE END Service1Char2_WRITE_NO_RESP_EVT */
```
For **Active Mode**, we simply read the value sent by the phone and save it!
**Active Mode - Do we just save it?** 
Yes! Think of `Active_Mode` as a "Settings" menu. When the phone writes "2" (Fall Detection) to Active Mode, we literally just save it to memory and do nothing else. Later, when the phone writes "0x01" to `System_Command`, our Radar Task wakes up, looks at the saved setting (`current_radar_mode`), and says *"Ah! They want Fall Detection! I will run that math."*
```c
    case CONTROL_SERVICE_ACTIVE_MODE_WRITE_NO_RESP_EVT:
      /* USER CODE BEGIN Service1Char1_WRITE_NO_RESP_EVT */
      if (p_Notification->DataTransfered.Length == 1) {
          current_radar_mode = p_Notification->DataTransfered.p_Payload[0];
      }
      /* USER CODE END Service1Char1_WRITE_NO_RESP_EVT */
```

### Part 3: Task Registration (Linking ID to Function)
**What this does:** In Step 1, we put `CFG_TASK_SEND_RADAR_DATA_ID` in a list. But the Sequencer doesn't know what C code to run when that ID is triggered! Registration solves this. `UTIL_SEQ_RegTask` takes the ID number, and permanently glues it to a function name (like `Radar_Process_Task`).

**ST Tutorial:** Inside `Custom_APP_Init()`, they link their task ID to their function.
```c
  /* USER CODE BEGIN CUSTOM_APP_Init */
  UTIL_SEQ_RegTask(1<< CFG_TASK_SW1_BUTTON_PUSHED_ID, UTIL_SEQ_RFU, Custom_Switch_c_Send_Notification);
  /* USER CODE END CUSTOM_APP_Init */
```

**Our Radar:** Inside `CONTROL_SERVICE_APP_Init()`, we link our radar task ID to our radar function.
```c
  /* USER CODE BEGIN Service1_APP_Init */
  UTIL_SEQ_RegTask(1<<CFG_TASK_SEND_RADAR_DATA_ID, UTIL_SEQ_RFU, Radar_Process_Task);
  /* USER CODE END Service1_APP_Init */
```

### Part 4: The Background Function (The Actual Work)
**What this does:** This is the C function that actually gets executed by the Sequencer. When you call `UTIL_SEQ_SetTask`, the Sequencer waits for a safe time, then executes everything inside this function. 

**ST Tutorial:** When the Sequencer runs their function, it packages `0x00` or `0x01` into an array and pushes it to the phone.
```c
void Custom_Switch_c_Send_Notification(void) {
  /* ... ST logic to toggle SW1_Status ... */
  Custom_STM_App_Update_Char(CUSTOM_STM_SWITCH_C, (uint8_t *)NotifyCharData);
}
```

**Our Radar:** We wrote `Radar_Process_Task()`. When the Sequencer runs this function, it will run the Acconeer radar math! Right now it is just an empty skeleton. Our next big mission is pasting the Acconeer code in here.
```c
/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
static void Radar_Process_Task(void) {
  if (is_radar_running == 0) return; 
  
  /* TODO: Based on 'current_radar_mode', call Acconeer radar math here */
  
  /* Keep the loop running */
  // UTIL_SEQ_SetTask(1<<CFG_TASK_SEND_RADAR_DATA_ID, CFG_SEQ_PRIO_0);
}
/* USER CODE END FD_LOCAL_FUNCTIONS */
```

### Part 5: Handling Notifications (Permission to Speak)
In BLE, you are not allowed to send a Notification until the phone "subscribes" to it. The ST tutorial and our Radar app handle this by setting a flag.

**ST Tutorial:** 
```c
    case CUSTOM_STM_SWITCH_C_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN CUSTOM_STM_SWITCH_C_NOTIFY_ENABLED_EVT */
      Custom_App_Context.Switch_c_Notification_Status = 1; 
      /* USER CODE END CUSTOM_STM_SWITCH_C_NOTIFY_ENABLED_EVT */
      break;
```

**Our Radar:** We do the same thing using the names generated by CubeMX.
```c
    case CONTROL_SERVICE_SENSOR_STATUS_NOTIFY_ENABLED_EVT:
      /* USER CODE BEGIN Service1Char3_NOTIFY_ENABLED_EVT */
      CONTROL_SERVICE_APP_Context.Sensor_status_Notification_Status = Sensor_status_NOTIFICATION_ON;
      /* USER CODE END Service1Char3_NOTIFY_ENABLED_EVT */
      break;
```
Once this flag is `ON`, your Background Function (Part 4) is allowed to call `CONTROL_SERVICE_UpdateValue()` to physically push new data packets to the phone!

---
## Summary of the "Glue"
To make the code above work, we also added two "C-language glue" items:
1. **Include:** `#include "stm32_seq.h"` (So we can use the Sequencer).
2. **Prototype:** `static void Radar_Process_Task(void);` (So the Init function knows the task exists at the bottom of the file).

---

## Step 5: The Low-Level GATT Handler
**File in ST Tutorial:** `custom_stm.c`
**File in our Radar App:** `control_service.c`

### What the ST Tutorial Does:
The tutorial asks you to go into the "Deep" GATT handler and manually write code to catch raw Bluetooth packets (`ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE`). They have to manually map the "Attribute Handle" to their "LED Characteristic" and then trigger a notification.

### How we adapt it for our Radar:
We **skip this completely!** 

In modern STM32CubeMX versions, ST improved their tools. CubeMX now generates all of this low-level code for you automatically. It catches the Bluetooth "Write" event in `control_service.c` and instantly sends it to our `control_service_app.c` file. 

This is why we were able to just write our `if (cmd == 0x01)` logic in the "App" file without touching the low-level "Service" file. CubeMX did the hard work for us!
