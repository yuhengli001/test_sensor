# Radar Adapter — Full Code Flow

End-to-end trace from MCU power-on through a complete vibration monitoring session to disconnect.

---

## Phase 0 — MCU Boot (one-time, at power-on)

Runs once when the STM32 powers up, before any phone connects.

```
main()  (Core/Src/main.c)
  ├─ MX_APPE_Config()          ← system clock, flash, power config
  ├─ SystemClock_Config()
  ├─ MX_GPIO_Init()
  ├─ MX_SPI2_Init()            ← SPI bus the A121 radar sits on
  ├─ MX_USART1/2_UART_Init()   ← debug UART
  └─ MX_APPE_Init()            (Core/Src/app_entry.c)
       └─ APP_BLE_Init()       (STM32_WPAN/App/app_ble.c)
            │
            ├─ BLE stack init + GATT server setup
            │
            ├─ UTIL_TIMER_Create(&Radar_Update_Timer_Id, 50ms, PERIODIC, Radar_Timer_Req)
            │    └─ timer created but NOT started — starts only on BLE connect
            │
            ├─ CONTROL_SERVICE_APP_Init()  (STM32_WPAN/App/control_service_app.c)
            │    ├─ CONTROL_SERVICE_Init()        ← register GATT service FE40 + chars FE41/42/43
            │    ├─ Radar_Adapter_Init()           ← register RSS HAL with Acconeer SDK
            │    │    ├─ acc_hal_rss_integration_get_implementation()
            │    │    └─ acc_rss_hal_register(hal) ← SDK now knows how to talk to A121 over SPI
            │    └─ UTIL_SEQ_RegTask(radar_task, Radar_Process_Task)
            │         └─ task registered but never SetTask'd — dormant
            │
            ├─ VIBRATION_SERVICE_APP_Init()  (STM32_WPAN/App/vibration_service_app.c)
            │    └─ VIBRATION_SERVICE_Init()  ← register GATT service FE70 + chars FE71/72/73
            │         └─ Vibration_Config initialised to defaults
            │              (preset=HIGH, point=80, hwaas=16, profile=3, csm=OFF, db=OFF)
            │
            └─ Start BLE advertising
                 └─ MCU is now discoverable — waiting for phone to connect
```

---

## Phase 1 — Phone Connects

**iOS** (`RadarBLEManager.swift`)

```
User taps CONNECT
  └─ startScanning()
       └─ centralManager.scanForPeripherals(withServices: nil)
            └─ didDiscover: name contains "test_sensor"
                 └─ connect(to: peripheral)
                      └─ didConnect
                           ├─ isConnected = true
                           └─ discoverServices([controlServiceUUID, vibrationServiceUUID, vitalSignServiceUUID])
                                └─ didDiscoverServices
                                     ├─ controlService   → discoverCharacteristics [FE41, FE42, FE43]
                                     └─ vibrationService → discoverCharacteristics [FE71, FE72]
                                          └─ didDiscoverCharacteristics
                                               ├─ FE41 → activeModeChar       (write mode)
                                               ├─ FE42 → systemCommandChar    (start/stop)
                                               ├─ FE71 → vibrationConfigChar  (write config)
                                               ├─ FE43 → setNotifyValue(true) (sensor status)
                                               └─ FE72 → setNotifyValue(true) (vibration data)
```

**Firmware** (`app_ble.c`)

```
BLE connection event received
  └─ UTIL_TIMER_Start(&Radar_Update_Timer_Id)
       └─ 50ms periodic timer now running
          (fires every 50ms, but radar task is a no-op since is_radar_running = 0)
```

---

## Phase 2 — Start Sensor

**iOS** (`VibrationDashboard.swift` → `RadarBLEManager.swift`)

```
User adjusts settings → only local @State vibConfig changes, nothing sent to MCU yet

User taps START MONITOR → toggleRadar()
  ├─ setMode(.vibration)
  │    └─ write 0x03 → FE41  (withoutResponse)
  ├─ sendVibrationConfig(vibConfig)
  │    └─ write 10 bytes → FE71  (withResponse)
  │         [preset(1) | measuredPoint(4) | hwaas(2) | profile(1) | csm(1) | db(1)]
  ├─ (200 ms delay — ensures mode + config arrive before START)
  └─ sendStartCommand()
       └─ write 0x01 → FE42  (withoutResponse)

isRunning = true  →  UI shows frequency display + displacement card
```

**Firmware** (`control_service_app.c`, `vibration_service_app.c`, `radar_adapter.c`)

```
FE41 write received  →  ACTIVE_MODE_WRITE_NO_RESP_EVT
  └─ current_radar_mode = 3  (RADAR_MODE_VIBRATION)

FE71 write received  →  VIBRATION_CONFIG_WRITE_EVT
  └─ memcpy(&Vibration_Config, payload, 10)   ← stored, not applied yet

FE42 write 0x01  →  SYSTEM_COMMAND_WRITE_NO_RESP_EVT
  ├─ is_radar_running = 1
  ├─ Radar_Adapter_Start(RADAR_MODE_VIBRATION)
  │    ├─ Radar_Adapter_Stop()              ← clean up any previous state first
  │    └─ init_vibration()
  │         ├─ VIBRATION_APP_GetConfig()    ← reads Vibration_Config stored above
  │         ├─ acc_vibration_preset_set()   ← load Acconeer HIGH/LOW defaults into ctx.vib_config
  │         ├─ apply user overrides
  │         │    ├─ ctx.vib_config.measured_point  ← hard overwrite
  │         │    ├─ ctx.vib_config.hwaas            ← hard overwrite
  │         │    └─ ctx.vib_config.profile          ← hard overwrite
  │         ├─ CSM / Double Buffering policy
  │         │    ├─ LOW_FREQ  → force both ON  (always required)
  │         │    └─ HIGH_FREQ → apply user values directly
  │         ├─ acc_vibration_handle_create(&ctx.vib_config)
  │         ├─ acc_processing_create()
  │         ├─ acc_rss_get_buffer_size() + acc_integration_mem_alloc()
  │         ├─ sensor_supply_on() + sensor_enable()
  │         ├─ acc_sensor_create(SENSOR_ID)
  │         └─ do_sensor_calibration_and_prepare()
  │                ├─ acc_sensor_calibrate() loop  (waits for interrupt each iteration)
  │                └─ acc_sensor_prepare()
  │
  └─ UTIL_SEQ_SetTask(radar_task)  ← kick off first measurement immediately
```

---

## Phase 3 — Measurement Loop

```
Every 50ms: Radar_Update_Timer fires  (app_ble.c → Radar_Timer_Req)
  └─ UTIL_SEQ_SetTask(radar_task, CFG_SEQ_PRIO_0)

UTIL_SEQ_Run picks up task → Radar_Process_Task()  (control_service_app.c)
  ├─ is_radar_running == 0? → return immediately (no-op)
  └─ Radar_Adapter_Process(RADAR_MODE_VIBRATION)  (radar_adapter.c)
       │
       ├─ acc_sensor_measure()
       │    └─ tell sensor to start one sweep sequence
       │
       ├─ wait_for_sensor_interrupt()
       │    └─ BLOCKS ~102 ms  (1024 sweeps ÷ 10000 Hz sweep rate)
       │       (UTIL_SEQ_Run called inside to keep BLE events alive during wait)
       │
       ├─ acc_sensor_read()
       │    └─ DMA completed frame data into ctx.buffer
       │
       ├─ acc_processing_execute()
       │    └─ RSS IQ processing — converts raw ADC → complex IQ samples
       │
       ├─ calibration_needed?
       │    └─ YES → do_sensor_calibration_and_prepare() + return false (skip frame)
       │
       ├─ acc_vibration_process(&proc_result, ctx.vib_handle, &ctx.vib_config, &result)
       │    └─ FFT on IQ time series at measured_point
       │       → result.peak_frequencies[]    (Hz)
       │       → result.peak_displacements[]  (µm)
       │
       ├─ stability filter  (only peak[0] considered)
       │    ├─ |freq - prev_freq| < 0.5 Hz → stability_counter++
       │    └─ else                         → stability_counter = 0
       │
       ├─ pass gates?  stability_counter >= 3  AND  displacement > 5 µm
       │    │
       │    ├─ NO, was previously stable → send one zero update to clear phone display
       │    │
       │    └─ YES →
       │         ├─ ω            = 2π × freq
       │         ├─ velocity     = displacement × ω / 1000      (µm·rad/s → mm/s)
       │         ├─ acceleration = displacement × ω² / 1e6      (µm·rad²/s² → m/s²)
       │         ├─ rms values   = peak / √2   (for disp, vel, accel)
       │         └─ VIBRATION_APP_UpdateData(freq, disp, disp_rms, vel, vel_rms, accel, accel_rms)
       │                └─ VIBRATION_SERVICE_UpdateValue(FE72)  ← BLE notify → phone
       │
       └─ return true
```

> **Timer vs frame period:** the 50ms timer fires twice per frame (~102ms). The second
> `SetTask` while the task is blocking in `wait_for_sensor_interrupt` re-marks it pending.
> When the frame completes the sequencer runs it again immediately. Effective throughput
> is **~1 frame per 102 ms (~10 fps)**, not 20 Hz.

**iOS receives BLE notification** (`RadarBLEManager.swift`)

```
didUpdateValueFor FE72
  └─ parseVibrationData(data)
       ├─ bytes 0–3 → frequency     (Float32 little-endian)
       └─ bytes 4–7 → displacement  (Float32 little-endian)
            └─ DispatchQueue.main.async { vibrationData = parsedData }
                 └─ SwiftUI redraws frequency display + displacement card
```

---

## Phase 4 — Stop Sensor

**iOS** (`VibrationDashboard.swift`)

```
User taps STOP SENSOR → toggleRadar()
  └─ sendStopCommand()
       └─ write 0x00 → FE42  (withoutResponse)

isRunning = false  →  metrics cards hidden, settings panel remains visible
```

**Firmware** (`control_service_app.c`, `radar_adapter.c`)

```
FE42 write 0x00  →  SYSTEM_COMMAND_WRITE_NO_RESP_EVT
  ├─ is_radar_running = 0
  └─ Radar_Adapter_Stop()
       ├─ acc_hal_integration_sensor_disable()
       ├─ acc_hal_integration_sensor_supply_off()
       ├─ acc_sensor_destroy()              → ctx.sensor = NULL
       ├─ acc_processing_destroy()          → ctx.processing = NULL
       ├─ acc_integration_mem_free()        → ctx.buffer = NULL
       ├─ reset: vib_stability_counter = 0, vib_prev_freq = 0, vib_was_stable = false
       └─ acc_vibration_handle_destroy()    → ctx.vib_handle = NULL

Next timer fire → Radar_Process_Task()
  └─ is_radar_running == 0 → return immediately
```

> **Timer is NOT stopped on STOP.** `Radar_Update_Timer` only stops on BLE disconnect
> (`app_ble.c`). It keeps firing every 50ms but each task call exits in one instruction.

> **STOP latency:** if the STOP command arrives while `wait_for_sensor_interrupt` is
> blocking mid-frame, `is_radar_running` cannot be set until the current task yields.
> STOP can take up to one full frame period (~102 ms) to take effect.

---

## Phase 5 — Disconnect

**iOS** (`RadarBLEManager.swift`)

```
User taps DISCONNECT  (or BLE supervision timeout)
  └─ centralManager.cancelPeripheralConnection()
       └─ didDisconnectPeripheral
            ├─ isConnected = false
            ├─ activeModeChar      = nil
            ├─ systemCommandChar   = nil
            └─ vibrationConfigChar = nil
```

**Firmware** (`app_ble.c`)

```
BLE disconnection event received
  └─ UTIL_TIMER_Stop(&Radar_Update_Timer_Id)
       └─ 50ms timer halted — Radar_Process_Task no longer scheduled
```

> Note: if the sensor was running when disconnect happens, `Radar_Adapter_Stop` is
> NOT automatically called from the disconnect handler. `ctx.sensor`, `ctx.buffer`,
> and `ctx.vib_handle` remain allocated until the next `Radar_Adapter_Start` calls
> `Radar_Adapter_Stop` as its first step.

---

## Full Lifecycle Summary

```
① MCU boot
     HAL registered with RSS, GATT services up, radar task registered, timer created

② Phone connects
     Timer starts (50ms periodic), task dormant (is_radar_running = 0)

③ Phone sends mode (FE41) + config (FE71) + START (FE42 = 0x01)
     init_vibration(): preset loaded → user overrides applied → sensor calibrated & prepared
     First radar task scheduled

④ Measurement loop
     Timer fires → task → [measure → wait ~102ms → read → FFT → filter → BLE notify]
     Repeats until stopped

⑤ Phone sends STOP (FE42 = 0x00)
     is_radar_running = 0, sensor destroyed and memory freed, task becomes no-op

⑥ Phone disconnects
     Timer stopped
```