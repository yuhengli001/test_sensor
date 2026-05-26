# Radar Sensor System — Reference Manual

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware](#2-hardware)
   - [2.1 Component Overview](#21-component-overview)
   - [2.2 STM32WBA64](#22-stm32wba64)
   - [2.3 Acconeer A121](#23-acconeer-a121)
   - [2.4 Power System](#24-power-system)
   - [2.5 PCB Layout](#25-pcb-layout)
   - [2.6 Debug Interface](#26-debug-interface)
3. [Firmware Architecture](#3-firmware-architecture)
   - [3.1 Source File Map](#31-source-file-map)
   - [3.2 UTIL_SEQ Cooperative Scheduler](#32-util_seq-cooperative-scheduler)
   - [3.3 Boot Sequence](#33-boot-sequence)
   - [3.4 State Variables](#34-state-variables)
4. [BLE Protocol](#4-ble-protocol)
   - [4.1 Service Overview](#41-service-overview)
   - [4.2 Control Service — FE40](#42-control-service--fe40)
   - [4.3 Vibration Service — FE70](#43-vibration-service--fe70)
   - [4.4 Vital Sign Service — FE50](#44-vital-sign-service--fe50)
   - [4.5 Start Sequence](#45-start-sequence)
   - [4.6 Notification Subscription](#46-notification-subscription)
   - [4.7 Current iOS App Implementation Status](#47-current-ios-app-implementation-status)
5. [Vibration Mode — Technical Reference](#5-vibration-mode--technical-reference)
   - [5.1 Signal Hierarchy: Pulse → Sweep → Frame](#51-signal-hierarchy-pulse--sweep--frame)
   - [5.2 Configuration Parameters](#52-configuration-parameters)
   - [5.3 Initialisation Sequence](#53-initialisation-sequence)
   - [5.4 Per-Frame Processing Loop](#54-per-frame-processing-loop)
   - [5.5 Signal Processing Algorithm](#55-signal-processing-algorithm)
   - [5.6 Post-Processing: Stability Filter and Output Gate](#56-post-processing-stability-filter-and-output-gate)
   - [5.7 Timing Summary](#57-timing-summary)
   - [5.8 Full Code Flow Reference](#58-full-code-flow-reference)
6. [Vital Signs Mode](#6-vital-signs-mode)
7. [Fall Detection Mode](#7-fall-detection-mode)
8. [iOS App](#8-ios-app)
   - [8.1 Project File Structure](#81-project-file-structure)
   - [8.2 RadarBLEManager](#82-radarbemanager)
   - [8.3 Data Models — RadarModels.swift](#83-data-models--radarmodelsswift)
   - [8.4 Vibration Dashboard — VibrationDashboard.swift](#84-vibration-dashboard--vibrationdashboardswift)
   - [8.5 Vital Signs Dashboard — VitalDashboard.swift](#85-vital-signs-dashboard--vitaldashboardswift)
   - [8.6 Fall Detection Dashboard](#86-fall-detection-dashboard)
   - [8.7 Shared UI Components](#87-shared-ui-components)

---

## 1. System Overview

This system is a battery-powered, BLE-connected radar sensing device capable of three operating modes: **vibration monitoring**, **vital sign detection** (breathing and heart rate), and **fall detection**. The device streams processed sensor data to an iOS companion app over Bluetooth Low Energy in real time.

### Architecture

![System Architecture Diagram](images/system_architecture.png)

### Operating Modes at a Glance

| Mode | What it measures | Status |
|---|---|---|
| Vibration | Dominant frequency (Hz), displacement (µm) at a configurable distance up to 1 m | Implemented |
| Vital Signs | Breathing rate (BPM), heart rate (BPM), distance (m) | Implemented |
| Fall Detection | Presence, sudden-drop event | Under development |

---

## 2. Hardware

### 2.1 Component Overview

| Component | Part | Role |
|---|---|---|
| MCU | STM32WBA64 | Application processor + BLE radio |
| Radar sensor | Acconeer A121 | 60.5 GHz pulsed coherent radar |
| Charging IC | TI BQ25176K | Single-cell Li-ion charger via USB |
| Battery | 3.7 V Li-ion, 2000 mAh | Primary power source |
| 3.3 V LDO | NCP167BMX330TBG | Powers USB-UART converter (VDD domain) |
| 1.8 V LDO | NCP167AMX180TBG | Powers MCU, A121, debug header (1V8 domain) |
| USB-UART converter | CP2105 | Debug UART and USB connectivity |
| BLE antenna | PCB trace | 2.4 GHz RF |
| Microwave Coaxial Connector | MM8130 | RF debug and test

---

### 2.2 STM32WBA64

| Specification | Value |
|---|---|
| Core | Arm Cortex-M33 with TrustZone |
| CPU frequency | 100 MHz |
| Flash | Up to 2 MB |
| RAM | Up to 512 KB (including 64 KB with parity) |
| Wireless | Bluetooth Low Energy 5.4 |
| BLE TX power | +10 dBm (max) |
| BLE RX sensitivity | −96 dBm @ 1 Mbps |
| Supply voltage | 1.71 V – 3.6 V |
| Key peripherals | SPI, UART, USB HS OTG, 86 GPIO |
| Operating temperature | −40 °C to +105 °C |
| Package (this design) | UFQFPN or UFBGA (see PCB) |

**Role in this system:** runs the BLE GATT server, the Acconeer RSS SDK, all signal processing, and the UTIL_SEQ cooperative task scheduler. Communicates with the A121 over SPI2.

---

### 2.3 Acconeer A121

| Specification | Value |
|---|---|
| Technology | Pulsed Coherent Radar (PCR) |
| Operating frequency | 60.5 GHz (57 – 64 GHz band) |
| Maximum range | 20 m |
| Interface | SPI (up to 50 MHz) |
| Supply voltage | 1.8 or 3.3V |
| IO power supply | 1.8 V |
| Active current | ~75 mA |
| Sleep current | ~3 mA |
| Idle current (ENABLE low) | < 1 µA |
| Operating temperature | −40 °C to +105 °C |
| Package | 50-VFBGA (5.4 × 5.4 mm, 500 µm pitch) |

**Role in this system:** performs all mmWave signal generation and reception. Internally executes hardware-accelerated sweep averaging (HWAAS) before passing IQ data to the STM32 over SPI. Controlled via three GPIO lines from the STM32: **ENABLE** (sensor power gate), **SPI CS/CLK/MOSI/MISO** (data), and **INTERRUPT** (signals frame-ready to STM32).

**A121 GPIO connections to STM32:**

| Signal | Direction | Description |
|---|---|---|
| ENABLE | STM32 → A121 | Powers the sensor on/off (supply gating) |
| SPI (CS, CLK, MOSI, MISO) | Bidirectional | Data interface (SPI2 on STM32) |
| INTERRUPT | A121 → STM32 | Asserted when a sweep/frame is ready to read |

---

### 2.4 Power System

The device operates from a **3.7 V / 2000 mAh single-cell Li-ion battery**, charged via USB using the **TI BQ25176K** linear charger IC. The battery output feeds to 1.8V LDO regulator that create the 1.8 V power supply domain for the MCU and A121 radar. The 3.3V LDO regulator provides the 3.3 V power supply domain for the USB-UART converter.

**Power domain diagram:**

![Power Domain Diagram](images/power_domain.png)                                    

**Key power notes:**
- The A121 is fully powered off by pulling ENABLE low — idle draw drops to < 1 µA, which the firmware uses between monitoring sessions to save battery
- The BQ25176K does not expose a digital SoC (state of charge) interface; battery percentage is not available to the firmware

---

### 2.5 PCB Layout

![PCB Layout](images/pcb_layout.png)

<!-- The PCB is a single compact board with the following major areas:

| Reference | Component | Location |
|---|---|---|
| U6 | STM32WBA64 MCU | Top-center |
| X1 (boxed area) | Acconeer A121 radar | Center |
| ANT1 | PCB trace BLE antenna | Top edge |
| U3 | Power IC | Lower-center |
| U2, U5 | Supporting ICs | Lower-left |
| LED1, LED2, LED3 | Status LEDs | Lower-left |
| RT1 | Thermistor | Lower-left |
| X2, X3 | Crystals (MCU clocks) | Center-left |
| D3, D4 | Diodes | Right edge | -->


### 2.6 Debug Interface

The board exposes two debug interfaces:

| Interface | Connector | Use |
|---|---|---|
| ST-Link SWD | ST-Link header (VDD, SWDIO, SWDCLK, GND) | Flash firmware, hardware breakpoint debug |
| Standard COM Port | USB-UART converter (USB Connector) | `LOG_INFO_APP` printf-style debug output at runtime |
| Enhanced COM Port | USB-UART converter (USB Connector) | Flash firmware |

UART debug output is the primary runtime diagnostic tool. All key firmware events (mode changes, sensor init, calibration, per-frame results) are printed via `LOG_INFO_APP` and are visible in any serial terminal at the baud rate (115200 baud).

---

## 3. Firmware Architecture

### 3.1 Source File Map

The firmware is built on top of the STM32 WPAN middleware and the Acconeer RSS SDK. The application layer sits in `STM32_WPAN/App/`:

| File | Role |
|---|---|
| `Core/Src/main.c` | Entry point — hardware peripheral init, calls `MX_APPE_Init()` |
| `Core/Src/app_entry.c` | Application entry — calls `APP_BLE_Init()` |
| `STM32_WPAN/App/app_ble.c` | BLE stack init, GATT event routing, `Radar_Update_Timer` (50 ms), connect/disconnect handlers |
| `STM32_WPAN/App/radar_adapter.c` | **Core abstraction layer** — owns all Acconeer RSS API calls, sensor lifecycle, and per-mode processing |
| `STM32_WPAN/App/control_service_app.c` | Handles BLE writes to FE41 (mode) and FE42 (start/stop), owns `is_radar_running` flag and `Radar_Process_Task` |
| `STM32_WPAN/App/control_service.c` | GATT characteristic definitions for Control Service (FE40) |
| `STM32_WPAN/App/vibration_service_app.c` | Stores `Vibration_Config`, handles FE71 config writes, sends FE72 BLE notifications |
| `STM32_WPAN/App/vibration_service.c` | GATT characteristic definitions for Vibration Service (FE70) |
| `cortex_m33_gcc/integration/` | Acconeer HAL integration — SPI, GPIO, sleep, interrupt wait |

**Acconeer SDK modules used:**

| Module | Purpose |
|---|---|
| `acc_rss_a121` | Core RSS init, HAL registration |
| `acc_sensor` | Sensor create/calibrate/prepare/measure/read/destroy |
| `acc_processing` | IQ data processing (raw buffer → complex IQ samples) |
| `acc_vibration` | Vibration algorithm — FFT, peak extraction, displacement calculation |
| `acc_detector_presence` | Presence detector used by vital signs and fall detection modes |

---

### 3.2 UTIL_SEQ Cooperative Scheduler

The firmware uses STM32's **UTIL_SEQ** cooperative scheduler — there is no preemption between application tasks. Tasks run to completion before the next one starts. The BLE stack events are processed inside `UTIL_SEQ_Run()`.

**Key entities:**

| Entity | Type | Period | Role |
|---|---|---|---|
| `Radar_Process_Task` | Task | On-demand | Calls `Radar_Adapter_Process()` — one full sensor frame per invocation |
| `Radar_Update_Timer` | Periodic timer | 50 ms | Fires `UTIL_SEQ_SetTask(radar_task)` — keeps measurements running |
| BLE HCI task | Task | Event-driven | Processes incoming BLE write commands (mode, config, start/stop) |

**Scheduling behaviour:**

```
50 ms timer fires
  └─ UTIL_SEQ_SetTask(radar_task)   ← marks task pending

UTIL_SEQ_Run() picks up radar_task
  └─ Radar_Process_Task()
       └─ Radar_Adapter_Process()
            └─ wait_for_sensor_interrupt()  ← blocks ~102 ms for vibration mode
                 └─ UTIL_SEQ_Run(HCI_ASYNCH_EVT) called inside the wait loop
                      └─ BLE write events (mode/config/start/stop) are processed here
                         while the sensor is acquiring a frame
```

The critical implication: the sensor's frame acquisition and BLE command processing are **interleaved inside `wait_for_sensor_interrupt`** — the CPU is never fully blocked from BLE events. However, a STOP command received mid-frame will not take effect until the current frame completes (~102 ms latency).

---

### 3.3 Boot Sequence

The following runs once at MCU power-on, before any phone connects:

```
main()
  ├─ Hardware peripheral init
  │    MX_GPIO_Init(), MX_SPI2_Init(), MX_USART1_UART_Init(), ...
  │
  └─ MX_APPE_Init()                          (app_entry.c)
       └─ APP_BLE_Init()                     (app_ble.c)
            │
            ├─ BLE stack + GATT server init
            │
            ├─ UTIL_TIMER_Create(Radar_Update_Timer, 50 ms, PERIODIC)
            │    └─ created but NOT started — starts only on BLE connect
            │
            ├─ CONTROL_SERVICE_APP_Init()    (control_service_app.c)
            │    ├─ CONTROL_SERVICE_Init()   ← register GATT service FE40
            │    ├─ Radar_Adapter_Init()
            │    │    ├─ acc_hal_rss_integration_get_implementation()
            │    │    └─ acc_rss_hal_register(hal)  ← RSS SDK ready to use SPI/GPIO
            │    └─ UTIL_SEQ_RegTask(radar_task, Radar_Process_Task)
            │
            ├─ VIBRATION_SERVICE_APP_Init()  (vibration_service_app.c)
            │    └─ VIBRATION_SERVICE_Init() ← register GATT service FE70
            │         └─ Vibration_Config initialised to defaults
            │              preset=HIGH, point=80, hwaas=16, profile=3, csm=OFF, db=OFF
            │
            └─ Start BLE advertising
```

**After boot:** the MCU is advertising and waiting for a phone to connect. The radar sensor is unpowered (`ENABLE` low, < 1 µA draw). No memory is allocated for sensor buffers yet — that happens in `init_vibration()` or `init_presence()` when the user taps START.

---

### 3.4 State Variables

Two global variables in `control_service_app.c` drive the runtime state machine:

| Variable | Type | Meaning |
|---|---|---|
| `current_radar_mode` | `uint8_t` | 0 = None, 1 = Vital, 2 = Fall, 3 = Vibration |
| `is_radar_running` | `uint8_t` | 0 = stopped, 1 = running |

`Radar_Process_Task` checks `is_radar_running` as its first instruction and returns immediately if it is 0 — this is how STOP takes effect without unregistering the task or stopping the timer.

---

## 4. BLE Protocol

### 4.1 Service Overview

The device exposes three GATT services. The iOS app discovers all three on connect and subscribes to notifications from status and data characteristics.

| Service | UUID | Purpose |
|---|---|---|
| Control Service | `0000FE40-CC7A-482A-984A-7F2ED5B3E58F` | Mode selection, start/stop, sensor status |
| Vibration Service | `0000FE70-CC7A-482A-984A-7F2ED5B3E58F` | Vibration config write, vibration data notify |
| Vital Sign Service | `0000FE50-CC7A-482A-984A-7F2ED5B3E58F` | Vital sign data notify |

---

### 4.2 Control Service — FE40

#### FE41 · Active Mode

| Field | Value |
|---|---|
| UUID | `0000FE41-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Write Without Response |
| Length | 1 byte |

| Byte | Meaning |
|---|---|
| `0x01` | Vital Sign mode |
| `0x02` | Fall Detection mode |
| `0x03` | Vibration mode |

Written before every START to tell the firmware which mode `Radar_Adapter_Start()` will initialise.

---

#### FE42 · System Command

| Field | Value |
|---|---|
| UUID | `0000FE42-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Write Without Response |
| Length | 1 byte |

| Byte | Meaning |
|---|---|
| `0x00` | STOP — sets `is_radar_running = 0`, calls `Radar_Adapter_Stop()` |
| `0x01` | START — sets `is_radar_running = 1`, calls `Radar_Adapter_Start(current_radar_mode)` |

---

#### FE43 · Sensor Status

| Field | Value |
|---|---|
| UUID | `0000FE43-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Notify |
| Length | 1 byte |

Lifecycle state notifications sent by the firmware. These are printed via `LOG_INFO_APP` and notified over BLE for diagnostic use — **not displayed in the iOS app UI**.

| Value | State |
|---|---|
| `0x00` | Powered Off |
| `0x01` | Initialized |
| `0x02` | Prepared |
| `0x03` | Measuring |
| `0x04` | Recalibrating |
| `0xFF` | Error |

---

### 4.3 Vibration Service — FE70

#### FE71 · Vibration Config

| Field | Value |
|---|---|
| UUID | `0000FE71-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Read, Write (with response) |
| Length | 10 bytes |

Written by the iOS app immediately before the START command. The firmware stores the payload in `Vibration_Config` and applies it inside `init_vibration()`.

**Byte layout (all multi-byte fields are little-endian):**

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0 | `preset` | `uint8_t` | 0 = High Frequency, 1 = Low Frequency |
| 1–4 | `measured_point` | `uint32_t` | Distance index; physical distance = `measured_point × 2.5 mm`. Range 1–400 (= 2.5 mm – 1000 mm). |
| 5–6 | `hwaas` | `uint16_t` | Hardware Accelerated Average Samples. Practical values: 8 / 16 / 32 / 64 / 128 / 256. |
| 7 | `profile` | `uint8_t` | Radar pulse profile 1–5. Higher = longer pulse, better SNR at range. |
| 8 | `continuous_sweep_mode` | `uint8_t` | 0 = OFF, 1 = ON. Ignored (forced ON) when `preset = 1`. |
| 9 | `double_buffering` | `uint8_t` | 0 = OFF, 1 = ON. Ignored (forced ON) when `preset = 1`. |

**Default values (applied at boot):**

| Field | Default |
|---|---|
| `preset` | 0 (High Frequency) |
| `measured_point` | 80 (= 200 mm) |
| `hwaas` | 16 |
| `profile` | 3 |
| `continuous_sweep_mode` | 0 (OFF) |
| `double_buffering` | 0 (OFF) |

---

#### FE72 · Vibration Data

| Field | Value |
|---|---|
| UUID | `0000FE72-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Notify |
| Length | 28 bytes |

Sent by the firmware after each processed frame that passes the stability and displacement gates. All values are IEEE 754 `float32`, little-endian.

| Bytes | Field | Unit | Notes |
|---|---|---|---|
| 0–3 | `frequency` | Hz | Dominant vibration peak frequency |
| 4–7 | `displacement` | µm | Peak displacement at `frequency` |
| 8–11 | `displacement_rms` | µm | RMS displacement (= peak / √2) |
| 12–15 | `velocity` | mm/s | Peak velocity (= displacement × ω / 1000) |
| 16–19 | `velocity_rms` | mm/s | RMS velocity |
| 20–23 | `acceleration` | m/s² | Peak acceleration (= displacement × ω² / 1×10⁶) |
| 24–27 | `acceleration_rms` | m/s² | RMS acceleration |

The iOS app currently reads only bytes 0–3 (`frequency`) and 4–7 (`displacement`). The remaining fields are computed and sent by the firmware for future use.

If the signal does not meet the stability gate (`stability_counter ≥ 3` AND `displacement > 5 µm`), the firmware sends a zero-value packet to clear the phone display.

---

#### FE73 · Spectrum Array

| Field | Value |
|---|---|
| UUID | `0000FE73-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Notify |
| Length | 240 bytes |

Reserved for future FFT spectrum streaming (frequency vs. displacement array). **Not used in the current firmware or iOS app.**

---

### 4.4 Vital Sign Service — FE50

#### FE52 · Vital Sign Data

| Field | Value |
|---|---|
| UUID | `0000FE52-8E22-4541-9D4C-21EDAE82ED19` |
| Properties | Notify |
| Length | 12 bytes |

Sent by the firmware after each processed vital sign frame. All values are IEEE 754 `float32`, little-endian.

| Bytes | Field | Unit |
|---|---|---|
| 0–3 | `breathing_rate` | Breaths per minute |
| 4–7 | `heart_rate` | Beats per minute |
| 8–11 | `distance` | Metres |

---

### 4.5 Start Sequence

Settings changed in the iOS UI are held in local app state only — they are not sent to the MCU immediately. They are applied exactly once, at the moment the user taps **START MONITOR**:

```
User taps START
      │
      ├─ 1. Write mode byte → FE41  (Write Without Response)
      │        value: 0x01 = Vital / 0x02 = Fall / 0x03 = Vibration
      │
      ├─ 2. Write config → FE71  (Write With Response, 10 bytes)   ← Vibration mode only
      │        [preset(1) | measured_point(4 LE) | hwaas(2 LE) | profile(1) | csm(1) | db(1)]
      │
      └─ (200 ms delay — ensures both writes arrive and are stored before START)
           │
           └─ 3. Write start command → FE42  (Write Without Response)
                    value: 0x01 = START
```

The 200 ms gap is required because FE41 and FE71 use different write types (no-response vs. with-response). Without the delay, the START command can arrive before the config has been stored by the firmware.

**Stop sequence** is a single write: `0x00 → FE42`. Any settings change while the sensor is running has no effect on the MCU until the next start cycle.

---

### 4.6 Notification Subscription

The iOS app enables notifications on two characteristics immediately after service/characteristic discovery:

| Characteristic | What triggers a notification |
|---|---|
| FE43 (Sensor Status) | Each lifecycle state transition in the firmware |
| FE72 (Vibration Data) | Each processed vibration frame (~10 fps, 100 ms period) |
| FE52 (Vital Sign Data) | Each processed vital sign frame |

Notifications are disabled automatically when the BLE connection drops; the firmware's periodic timer is also stopped at that point (`UTIL_TIMER_Stop` in the disconnect handler in `app_ble.c`).

---

### 4.7 Current iOS App Implementation Status

The firmware exposes more data and accepts more control parameters than the iOS app currently uses. The table below documents what is wired up in the current release versus what is defined but not yet consumed.

#### Characteristics

| Char | Firmware exposes | iOS app (current) | Notes |
|---|---|---|---|
| FE41 | Mode selection (Vital / Fall / Vibration) | Used — Vital and Vibration | Fall mode byte sent but dashboard is a placeholder |
| FE42 | START (0x01) / STOP (0x00) | Used | |
| FE43 | Sensor lifecycle state (7 values) | Subscribed, not displayed | Visible in STM32 debug UART log only |
| FE71 | Full 10-byte vibration config | Used — all 6 fields written on START | |
| FE72 | 28 bytes — 7 float32 metrics | Partially used | Only `frequency` (bytes 0–3) and `displacement` (bytes 4–7) shown in the UI; `displacement_rms`, `velocity`, `velocity_rms`, `acceleration`, `acceleration_rms` are received but discarded |
| FE73 | 240-byte FFT spectrum array | Not used | Reserved for future frequency-vs-displacement chart |
| FE52 | 12 bytes — 3 float32 vital metrics | Used — all 3 fields displayed | `breathing_rate`, `heart_rate`, `distance` all shown on Vital Signs dashboard |

#### Mode-Specific Configuration (Phone → Sensor)

| Mode | Config characteristic | iOS app (current) |
|---|---|---|
| Vibration | FE71 — preset, range, HWAAS, profile, CSM, DB | All parameters implemented in settings panel |
| Vital Signs | None defined yet | No config sent — `start_m`, `end_m`, sensitivity and other parameters are under development |
| Fall Detection | None defined yet | Mode is a placeholder — no config or live data |

---

## 5. Vibration Mode — Technical Reference

### 5.1 Signal Hierarchy: Pulse → Sweep → Frame

Understanding the three levels of signal acquisition is essential for reasoning about any vibration parameter.

```
Pulse
  └─ One transmitted mmWave burst + one received echo
     Produced entirely inside A121 silicon (invisible to the STM32)

Sweep
  └─ One averaged sample delivered to STM32 over SPI
     A121 fires N pulses internally (N = HWAAS), averages them, returns one result
     Random noise partially cancels; real reflections add coherently → +3 dB per doubling of HWAAS

Frame
  └─ sweeps_per_frame sweeps stacked into a time-series buffer
     Handed to acc_vibration_process() for FFT analysis
     frame_period ≈ sweeps_per_frame / sweep_rate
```

**High Frequency preset (default):** 1024 sweeps at 10 000 Hz sweep rate → frame period ≈ 102 ms (~10 fps).  
**Low Frequency preset:** 20 sweeps at 200 Hz sweep rate → frame period ≈ 100 ms (~10 fps).

The FFT is computed over the time series of IQ phase values at `measured_point`. Frequency resolution = `sweep_rate / time_series_length` = 10 000 / 1024 ≈ **9.8 Hz** for High Frequency preset, and 200 / 1024 ≈ **0.2 Hz** for Low Frequency preset.

---

### 5.2 Configuration Parameters

#### User-Controlled (sent over FE71)

| Parameter | Type | Default | Range | Effect |
|---|---|---|---|---|
| `preset` | enum | HIGH | HIGH / LOW | Loads the locked parameters below. Must be set first; overrides follow. |
| `measured_point` | uint32 | 80 | 1 – 400 | Distance index to monitor. Physical distance = `measured_point × 2.5 mm` (point 80 = 200 mm, point 400 = 1000 mm). |
| `hwaas` | uint16 | 16 | 8 / 16 / 32 / 64 / 128 / 256 | Hardware-averaged samples per sweep. Each doubling adds ~+3 dB SNR. Use higher values for longer detection distances. |
| `profile` | uint8 | 3 | 1 – 5 | Radar pulse duration. Higher profile = more energy per sweep = better SNR at range. No spatial resolution penalty in single-point vibration mode. |
| `continuous_sweep_mode` | bool | OFF (HIGH) | ON / OFF | Forces uniform inter-sweep intervals. Required for accurate FFT at low frequencies. Always ON in LOW preset. |
| `double_buffering` | bool | OFF (HIGH) | ON / OFF | Prevents CPU readout from stalling the sweep stream. Must be paired with CSM. Always ON in LOW preset. |

#### Locked / Hardcoded per Preset

| Parameter | High Frequency | Low Frequency | Notes |
|---|---|---|---|
| `sweep_rate` | 10 000 Hz | 200 Hz | Sets Nyquist frequency (max detectable = sweep_rate / 2: 5000 Hz or 100 Hz) |
| `sweeps_per_frame` | 1024 | 20 | Keeps frame rate at ~10 fps for both presets |
| `time_series_length` | 1024 | 1024 | FFT input length; freq. resolution = sweep_rate / 1024 |
| `lp_coeff` | 0.5 | 0.8 | Low-pass filter coefficient applied to the IQ phase time series |
| `low_frequency_enhancement` | false | true | Boosts weak low-frequency FFT components |
| `amplitude_threshold` | 100.0 | 100.0 | Minimum signal amplitude to report a peak |
| `frame_rate` | 0 (unconstrained) | 0 | Sensor runs as fast as hardware allows |
| `inter_frame_idle_state` | READY | READY | No power saving between frames |
| `inter_sweep_idle_state` | READY | READY | No power saving between sweeps |

#### CSM and Double Buffering — Preset Policy

```c
// radar_adapter.c — init_vibration()
if (preset == ACC_VIBRATION_PRESET_LOW_FREQUENCY) {
    ctx.vib_config.continuous_sweep_mode = true;   // always forced ON
    ctx.vib_config.double_buffering      = true;   // always forced ON
} else {
    ctx.vib_config.continuous_sweep_mode = (cfg->continuous_sweep_mode != 0);
    ctx.vib_config.double_buffering      = (cfg->double_buffering != 0);
}
```

LOW_FREQUENCY always forces both ON regardless of what the iOS app sends. HIGH_FREQUENCY passes user values through. The iOS UI reflects this by locking both toggles to ON (greyed out) when the Low Frequency preset is selected.

---

### 5.3 Initialisation Sequence

Called from `Radar_Adapter_Start(RADAR_MODE_VIBRATION)` when the START command (FE42 = 0x01) is received.

```
init_vibration()
  │
  ├─ 1. Read Vibration_Config from vibration_service_app (stored when FE71 was written)
  │
  ├─ 2. acc_vibration_preset_set(&ctx.vib_config, preset)
  │       Loads all locked parameters for HIGH or LOW into ctx.vib_config
  │
  ├─ 3. Apply user overrides
  │       ctx.vib_config.measured_point          ← hard overwrite
  │       ctx.vib_config.hwaas                   ← hard overwrite
  │       ctx.vib_config.profile                 ← hard overwrite
  │       ctx.vib_config.continuous_sweep_mode   ← preset policy (see above)
  │       ctx.vib_config.double_buffering        ← preset policy
  │       ctx.vib_config.reported_displacement_mode ← always AMPLITUDE
  │
  ├─ 4. acc_vibration_handle_create(&ctx.vib_config)  → ctx.vib_handle
  │
  ├─ 5. acc_processing_create(sensor_config, &ctx.proc_meta)  → ctx.processing
  │
  ├─ 6. acc_rss_get_buffer_size() + acc_integration_mem_alloc()  → ctx.buffer
  │
  ├─ 7. sensor_supply_on() + sensor_enable()  (ENABLE line pulled high → A121 powers up)
  │
  ├─ 8. acc_sensor_create(SENSOR_ID)  → ctx.sensor
  │
  └─ 9. do_sensor_calibration_and_prepare()
          ├─ acc_sensor_calibrate() loop  (waits for INTERRUPT each iteration)
          └─ acc_sensor_prepare()
```

After this sequence completes, `UTIL_SEQ_SetTask(radar_task)` is called to kick off the first measurement frame immediately, without waiting for the 50 ms timer.

---

### 5.4 Per-Frame Processing Loop

Runs inside `Radar_Adapter_Process(RADAR_MODE_VIBRATION)`, called once per `Radar_Process_Task` invocation.

```
acc_sensor_measure(ctx.sensor)
  └─ Commands the A121 to begin acquiring sweeps_per_frame sweeps

acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, 1000 ms timeout)
  └─ Blocks the task until the A121 asserts its INTERRUPT line (frame ready)
     Duration: ~102 ms for High Frequency preset (1024 sweeps ÷ 10 000 Hz)
     BLE events are still processed during this wait via UTIL_SEQ_Run() inside the loop

acc_sensor_read(ctx.sensor, ctx.buffer, ctx.buffer_size)
  └─ DMA transfers the completed frame from A121 into ctx.buffer over SPI2

acc_processing_execute(ctx.processing, ctx.buffer, &proc_result)
  └─ RSS IQ processing: raw ADC samples → complex IQ time series
     proc_result.calibration_needed? → re-calibrate and skip frame

acc_vibration_process(&proc_result, ctx.vib_handle, &ctx.vib_config, &result)
  └─ FFT on the IQ phase time series at measured_point
     → result.peak_frequencies[]    (Hz)
     → result.peak_displacements[]  (µm, reported as amplitude)
     → result.peak_count
```

---

### 5.5 Signal Processing Algorithm

This section explains what happens mathematically inside `acc_processing_execute` and `acc_vibration_process` — the two RSS calls that turn a raw frame buffer into a vibration frequency and displacement reading.

#### Step 1 — Raw IQ Data

The A121 radar measures the echo of each transmitted pulse as a **complex IQ (In-phase + Quadrature) sample**. For a given range point, the IQ value encodes both the amplitude and the phase of the reflected signal:

```
IQ sample = I + jQ
  amplitude = sqrt(I² + Q²)   ← signal strength at that distance
  phase     = atan2(Q, I)     ← encodes the round-trip distance to the target
```

`acc_processing_execute` handles this conversion from the raw DMA buffer into a structured array of complex IQ values indexed by range point. The result is `proc_result`, which contains one complex IQ value per range point per sweep in the frame.

#### Step 2 — Phase Extraction at `measured_point`

Vibration mode focuses on a **single range point** (`measured_point`). The displacement of the target from one sweep to the next causes the round-trip distance to change slightly, which shifts the phase of the IQ sample at that point.

The relationship between phase change and physical displacement:

```
Δphase = (4π × Δdistance) / λ

where λ = c / f_radar = 3×10⁸ m/s / 60.5×10⁹ Hz ≈ 5 mm (carrier wavelength)
```

A 1 µm displacement produces a phase shift of roughly **0.0025 rad** — small but measurable with coherent radar. This is why the A121 can detect sub-millimetre vibrations.

`acc_vibration_process` extracts the phase at `measured_point` from each sweep in the frame, building a **phase time series** of length `sweeps_per_frame` (1024 for High Frequency preset).

#### Step 3 — Phase Unwrapping and Low-Pass Filtering

The raw phase is bounded to **[−π, π]**. When the target moves more than λ/4 ≈ 1.25 mm in one inter-sweep interval, the phase wraps around and produces a false discontinuity. The algorithm unwraps the phase by detecting and correcting these jumps, reconstructing the true continuous displacement waveform.

A **low-pass filter** (coefficient `lp_coeff`) is then applied to the unwrapped phase time series to suppress high-frequency electronic noise before the FFT step. The `lp_coeff` defaults differ by preset:

| Preset | `lp_coeff` | Reason |
|---|---|---|
| High Frequency | 0.5 | Less smoothing — fast signals need less filtering |
| Low Frequency | 0.8 | More smoothing — slow signals need stronger noise rejection |

#### Step 4 — FFT → Frequency and Displacement

An FFT is computed over the filtered phase time series. Each frequency bin in the output corresponds to a vibration frequency; the magnitude of each bin is proportional to the displacement amplitude at that frequency.

```
frequency_resolution = sweep_rate / time_series_length

High Frequency preset: 10 000 / 1024 ≈ 9.8 Hz per bin
Low Frequency preset:    200 / 1024 ≈ 0.2 Hz per bin

max_detectable_frequency = sweep_rate / 2   (Nyquist)
High Frequency preset: 5000 Hz
Low Frequency preset:   100 Hz
```

`acc_vibration_process` identifies peaks in the FFT output and converts their bin magnitudes back to displacement in µm, returning `result.peak_frequencies[]` and `result.peak_displacements[]`.

The `low_frequency_enhancement` flag (ON for Low Frequency preset) applies additional gain to the lower FFT bins to compensate for the natural roll-off that makes slow, small-amplitude signals harder to detect.

#### Summary of the Full Chain

```
A121 raw buffer (IQ samples, 1024 sweeps × N range points)
  │
  ├─ acc_processing_execute()
  │    └─ Raw ADC → complex IQ values per sweep per range point
  │
  └─ acc_vibration_process()
       ├─ Extract phase time series at measured_point  (atan2 on each sweep's IQ)
       ├─ Unwrap phase  (correct ±π jumps)
       ├─ Low-pass filter  (lp_coeff)
       ├─ FFT  (length = time_series_length = 1024)
       ├─ Peak detection  (find dominant frequency bins)
       └─ Convert bin magnitude → displacement (µm)
            → result.peak_frequencies[]
            → result.peak_displacements[]
```

---

### 5.6 Post-Processing: Stability Filter and Output Gate

This layer sits above the signal processing algorithm in `radar_adapter.c`. Once `acc_vibration_process` has returned peak frequencies and displacements, the firmware applies its own application-level filter before sending anything over BLE — raw FFT results are never sent directly to the phone.

```
Frame arrives with result.peak_count > 0
  │
  ├─ Frequency stability check:
  │    |current_freq - prev_freq| < 0.5 Hz  →  stability_counter++
  │    else                                 →  stability_counter = 0
  │
  └─ Pass gates?  stability_counter ≥ 3  AND  current_displacement > 5 µm
       │
       ├─ YES → compute derived metrics and send BLE notification:
       │          ω            = 2π × freq
       │          velocity     = displacement × ω / 1000         (µm·rad/s → mm/s)
       │          acceleration = displacement × ω² / 1 000 000   (µm·rad²/s² → m/s²)
       │          rms values   = peak / √2
       │          VIBRATION_APP_UpdateData(freq, disp, disp_rms, vel, vel_rms, accel, accel_rms)
       │               └─ VIBRATION_SERVICE_UpdateValue(FE72)  ← BLE notify → phone
       │
       ├─ NO (was previously stable) → send one zero packet to clear the phone display
       │          VIBRATION_APP_UpdateData(0, 0, 0, 0, 0, 0, 0)
       │
       └─ NO (was already unstable) → do nothing
```

**Gate logic summary:**

| Condition | Effect |
|---|---|
| Frequency stable for ≥ 3 consecutive frames AND displacement > 5 µm | Normal output — BLE notify with live data |
| Below threshold or unstable, previously was stable | One zero notify sent to clear the app display |
| Below threshold or unstable, already cleared | Silent — no BLE write |

**Debug log output** (printed every 20 frames ≈ 1 second via `LOG_INFO_APP`):

```
[VIB RAW] peaks=1  #0: freq=120.45 Hz  disp=38.20 um  stability=5/3  disp_ok=YES
[FILTERED VIB] Freq=120.45 Hz  Disp=38.20 um  Vel=28.92 mm/s  Accel=21881.34 m/s^2
```

---

### 5.7 Timing Summary

| Parameter | High Frequency | Low Frequency |
|---|---|---|
| Sweep rate | 10 000 Hz | 200 Hz |
| Sweeps per frame | 1024 | 20 |
| Frame acquisition time | ~102 ms | ~100 ms |
| Effective output rate | ~10 fps | ~10 fps |
| Max detectable frequency | 5000 Hz | 100 Hz |
| FFT frequency resolution | ~9.8 Hz | ~0.2 Hz |
| Min detectable frequency | ~10 Hz | ~0.2 Hz |

The 50 ms `Radar_Update_Timer` fires **twice per frame acquisition period**. The second `SetTask` call while the task is blocking in `wait_for_sensor_interrupt` simply re-marks the task pending — it does not stack or queue a second execution. Effective throughput is **one frame per ~102 ms**, not 20 Hz.

---

### 5.8 Full Code Flow Reference

The complete end-to-end code flow for a vibration session — from phone connect through start, measurement loop, stop, and disconnect — is documented in [`radar_code_flow.md`](../radar_code_flow.md).

---

## 6. Vital Signs Mode

> **Algorithm implemented, configuration UI under development.** See `radar_adapter.c` (`init_presence`, `RADAR_MODE_VITAL` branch) for the current firmware implementation. BLE config characteristic not yet defined.

---

## 7. Fall Detection Mode

> **Under development.** The presence detector infrastructure is shared with Vital Signs mode. Fall event classification logic is not yet implemented in firmware or iOS app.

---

## 8. iOS App

### 8.1 Project File Structure

The iOS app is a SwiftUI project with six source files. All BLE state is owned by a single shared manager instance.

| File | Role |
|---|---|
| `RadarProApp.swift` | App entry point — creates the SwiftUI app lifecycle |
| `MainTabView.swift` | Root view — owns the single `RadarBLEManager` instance (@StateObject), hosts the 3-tab TabView |
| `RadarBLEManager.swift` | **Core BLE layer** — CoreBluetooth central manager, service/characteristic discovery, all read/write/notify logic |
| `RadarModels.swift` | Shared data models — `VibrationData`, `VitalData`, `VibrationConfig`, `RadarMode`, `SystemCommand` |
| `VibrationDashboard.swift` | Vibration tab UI — settings panel, frequency/displacement display, START/STOP control |
| `VitalDashboard.swift` | Vital Signs tab UI — heart rate, breathing rate, distance display, START/STOP control |

`RadarBLEManager` is instantiated once in `MainTabView` as a `@StateObject` and passed down to each dashboard as `@ObservedObject`. This means BLE connection state and live data are preserved when the user switches tabs.

---

### 8.2 RadarBLEManager

`RadarBLEManager` is an `ObservableObject` that conforms to both `CBCentralManagerDelegate` and `CBPeripheralDelegate`. It is the only file that touches CoreBluetooth.

#### Published State

| Property | Type | Description |
|---|---|---|
| `isConnected` | `Bool` | True when a peripheral is fully connected and characteristics are discovered |
| `isScanning` | `Bool` | True while the central manager is scanning for peripherals |
| `vibrationData` | `VibrationData` | Latest frequency + displacement from FE72 notifications |
| `vitalData` | `VitalData` | Latest breathing rate, heart rate, distance from FE52 notifications |
| `sensorStatus` | `UInt8` | Latest lifecycle state byte from FE43 notifications |

#### Private Characteristic References

| Property | Characteristic | Used for |
|---|---|---|
| `activeModeChar` | FE41 | `setMode()` writes |
| `systemCommandChar` | FE42 | `sendStartCommand()` / `sendStopCommand()` writes |
| `vibrationConfigChar` | FE71 | `sendVibrationConfig()` writes |

FE43, FE72, and FE52 are notify-only; references are not stored — `setNotifyValue(true, ...)` is called on discovery and the manager receives updates via `didUpdateValueFor`.

#### Connection Flow

```
startScanning()
  └─ centralManager.scanForPeripherals(withServices: nil)
       └─ didDiscover: name.contains("test_sensor") → connect(to: peripheral)
            └─ didConnect → discoverServices([FE40, FE70, FE50])
                 └─ didDiscoverServices
                      ├─ FE40 → discoverCharacteristics([FE41, FE42, FE43])
                      ├─ FE70 → discoverCharacteristics([FE71, FE72])
                      └─ FE50 → discoverCharacteristics([FE52])
                           └─ didDiscoverCharacteristics
                                ├─ FE41 → activeModeChar
                                ├─ FE42 → systemCommandChar
                                ├─ FE43 → setNotifyValue(true)
                                ├─ FE71 → vibrationConfigChar
                                ├─ FE72 → setNotifyValue(true)
                                └─ FE52 → setNotifyValue(true)
```

#### Command Methods

| Method | BLE write | Type |
|---|---|---|
| `setMode(_ mode: RadarMode)` | 1 byte → FE41 | Write Without Response |
| `sendStartCommand()` | `0x01` → FE42 | Write Without Response |
| `sendStopCommand()` | `0x00` → FE42 | Write Without Response |
| `sendVibrationConfig(_ config: VibrationConfig)` | 10 bytes → FE71 | Write With Response |

#### Notification Parsing

`parseVibrationData(_ data: Data)` — reads 8 bytes minimum; extracts two `Float32` little-endian values:
- bytes 0–3 → `frequency` (Hz)
- bytes 4–7 → `displacement` (µm)

The remaining 20 bytes of the 28-byte FE72 payload (disp_rms, velocity, vel_rms, acceleration, accel_rms) are received but not currently read.

`parseVitalData(_ data: Data)` — reads 12 bytes; extracts three `Float32` little-endian values:
- bytes 0–3 → `breathingBpm`
- bytes 4–7 → `heartBpm`
- bytes 8–11 → `distance` (m)

---

### 8.3 Data Models — RadarModels.swift

#### VibrationData
Lightweight struct holding the two values currently displayed in the Vibration dashboard. Created fresh on each FE72 notification.

#### VitalData
Lightweight struct holding the three values displayed in the Vital Signs dashboard. Created fresh on each FE52 notification.

#### VibrationConfig
Mirrors the firmware's packed `VIBRATION_Config_t` exactly. Written to FE71 on every START.

| Field | Swift type | Default | Firmware field |
|---|---|---|---|
| `preset` | `UInt8` | 0 (HIGH) | `preset` |
| `measuredPoint` | `UInt32` | 80 | `measured_point` |
| `hwaas` | `UInt16` | 16 | `hwaas` |
| `profile` | `UInt8` | 3 | `profile` |
| `continuousSweepMode` | `UInt8` | 0 | `continuous_sweep_mode` |
| `doubleBuffering` | `UInt8` | 0 | `double_buffering` |

`distanceMeters` is a computed property — a convenience accessor for `measuredPoint` in metres (= `measuredPoint × 0.0025`). It is not serialised; only `measuredPoint` goes over BLE.

`toData()` packs the struct into exactly 10 bytes, all multi-byte fields in little-endian order, matching the firmware `memcpy` target.

#### Enums

```swift
enum RadarMode: UInt8 { case standby = 0, vitalSign = 1, fallDetection = 2, vibration = 3 }
enum SystemCommand: UInt8 { case stop = 0, start = 1 }
```

---

### 8.4 Vibration Dashboard — VibrationDashboard.swift

<table>
  <tr>
    <td align="center"><img src="images/1.png" width="220"/></td>
    <td align="center"><img src="images/2.png" width="220"/></td>
    <td align="center"><img src="images/3.png" width="220"/></td>
  </tr>
</table>

#### State

| Property | Type | Description |
|---|---|---|
| `bleManager` | `@ObservedObject RadarBLEManager` | Shared BLE manager from MainTabView |
| `isRunning` | `@State Bool` | Tracks whether a monitoring session is active |
| `vibConfig` | `@State VibrationConfig` | Local copy of settings; only sent on START |

#### Layout (when connected)

```
header              ← "RADAR SENSOR / Vibration" + CONNECT/DISCONNECT button
frequencyDisplay    ← large readout of bleManager.vibrationData.frequency (Hz)
displacementCard    ← MetricCard: bleManager.vibrationData.displacement (µm)
settingsSection     ← sensor settings panel (scrollable)
controlButton       ← START MONITOR / STOP SENSOR (pinned at bottom)
```

When disconnected: `emptyState` is shown instead (icon + "No Radar Connected" + optional scan spinner).

#### Settings Panel Controls

| Control | SwiftUI component | Drives |
|---|---|---|
| Frequency Mode | `.menu` Picker | `vibConfig.preset` (HIGH = 0, LOW = 1) |
| Detection Range | Slider (0.1–1.0 m, step 0.025) | `vibConfig.distanceMeters` → `vibConfig.measuredPoint` |
| Pulse Profile | `.segmented` Picker (1–5) | `vibConfig.profile` |
| HWAAS | `.menu` Picker (8/16/32/64/128/256) | `vibConfig.hwaas` |
| Continuous Sweep | Toggle | `vibConfig.continuousSweepMode`; disabled + dimmed when `preset == 1` |
| Double Buffering | Toggle | `vibConfig.doubleBuffering`; disabled + dimmed when `preset == 1` |

The subtitle under each control label ("applies on next start") and the locked state for CSM/DB when Low Frequency is selected mirror the firmware's CSM/DB policy.

#### Start Sequence (toggleRadar)

```swift
// isRunning == false → START path
bleManager.setMode(.vibration)                     // write 0x03 → FE41
bleManager.sendVibrationConfig(vibConfig)          // write 10 bytes → FE71 (withResponse)
DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
    bleManager.sendStartCommand()                  // write 0x01 → FE42 after 200 ms
}
isRunning = true

// isRunning == true → STOP path
bleManager.sendStopCommand()                       // write 0x00 → FE42
isRunning = false
```

Settings changes while running only update local `vibConfig` — the MCU is not notified until the next START.

---

### 8.5 Vital Signs Dashboard — VitalDashboard.swift

<table>
  <tr>
    <td align="center"><img src="images/4.png" width="220"/></td>
  </tr>
</table>

#### State

| Property | Type | Description |
|---|---|---|
| `bleManager` | `@ObservedObject RadarBLEManager` | Shared BLE manager |
| `isRunning` | `@State Bool` | Tracks whether a monitoring session is active |

No config struct — vital signs mode has no user-configurable parameters in the current implementation.

#### Layout (when connected)

```
header              ← "RADAR SENSOR / Vital Signs" + CONNECT/DISCONNECT button
heart icon          ← pulsing animation while isRunning
heartBpm display    ← large readout (red)
breathingBpm card   ← VitalMetricCard (blue)
distance card       ← VitalMetricCard (orange)
START/STOP button
```

#### Start Sequence (toggleVitals)

```swift
// START path — no config write, just mode + start
bleManager.setMode(.vitalSign)                    // write 0x01 → FE41
DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
    bleManager.sendStartCommand()                 // write 0x01 → FE42
    isRunning = true
}

// STOP path
bleManager.sendStopCommand()                      // write 0x00 → FE42
isRunning = false
```

Note the 100 ms delay (half of the vibration dashboard's 200 ms) — there is no config write for this mode, so the gap only needs to cover the mode byte arriving before START.

---

### 8.6 Fall Detection Dashboard

Displayed as a `PlaceholderView` — shows a hammer icon, the text "Fall Detection", and the subtitle "Algorithm Integration Pending". No BLE writes, no live data, no controls.

---

### 8.7 Shared UI Components

#### MetricCard
Defined at the bottom of `VibrationDashboard.swift`. Used by both the Vibration and Vital Signs dashboards for secondary metric display. Parameters: `title`, `value`, `unit`, `color`. Renders a left-aligned label + large value + coloured unit tag on a dark frosted card background.

#### VitalMetricCard
Defined at the bottom of `VitalDashboard.swift`. Functionally identical to MetricCard; kept separate to avoid cross-file dependency. Candidate for consolidation into a shared component file in a future refactor.

#### Theme Constants

| Element | Value |
|---|---|
| Background colour | `Color(red: 0.05, green: 0.05, blue: 0.07)` (`#0D0D12`) |
| Tab bar background | `UIColor(red: 0.05, green: 0.05, blue: 0.07, alpha: 1.0)` |
| Colour scheme | `.dark` (forced via `.preferredColorScheme(.dark)`) |
| Vibration accent | `.blue` |
| Vital Signs accent | `.red` |

---
