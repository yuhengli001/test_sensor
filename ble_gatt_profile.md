# BLE GATT Profile Design (Radar Application)

To keep the STM32 firmware clean and the mobile app efficient, we will separate the features into distinct BLE Services. We will use packed C-structs (byte arrays) for configuration and data to avoid creating dozens of individual characteristics.

*Note: The UUIDs below use a custom 128-bit base UUID `1234xxxx-5678-9abc-def0-123456789abc`. You can replace this with any random UUID generator later.*

---

## 1. Radar Control Service
**Service UUID:** `12340001-...`
Manages the global state of the sensor.

| Characteristic | UUID | Properties | Data Type (C struct / Size) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Active Mode** | `...0002` | Read, Write | `uint8_t` (1 byte) | `0` = Standby<br>`1` = Vital/Distance<br>`2` = Fall Detection<br>`3` = Vibration |
| **System Command**| `...0003` | Write | `uint8_t` (1 byte) | `0` = Stop (Off)<br>`1` = Start (Measure) |
| **Sensor Status** | `...0004` | Read, Notify | `uint8_t` (1 byte) | `0`=Powered Off, `1`=Initialized, `2`=Prepared, `3`=Measuring, `4`=Recalibrating, `5`=Error |

---

## 2. Vital Sign / Distance Service
**Service UUID:** `12341000-...`
Handles data and configuration specifically for Mode 1.

| Characteristic | UUID | Properties | Data Type (C struct / Size) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Distance Config** | `...1001` | Read, Write | `struct` (~40 bytes) | A packed struct containing all 13 Acconeer parameters (`start_m`, `end_m`, etc.). App writes this before sending the Start command. |
| **Vital Data** | `...1002` | Notify | `struct` (12 bytes) | `float rpm`<br>`float distance_m`<br>`float signal_quality` |
| **Waveform** *(Opt)*| `...1003` | Notify | `float array` | Live displacement array (only notify if active). |

---

## 3. Fall Detection Service
**Service UUID:** `12342000-...`
Handles data and configuration specifically for Mode 2.

| Characteristic | UUID | Properties | Data Type (C struct / Size) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Fall Config** | `...2001` | Read, Write | `struct` (TBD) | Contains range, sensitivity, etc. |
| **Fall Data** | `...2002` | Notify | `struct` (10 bytes) | `uint8_t fall_alert` (0/1)<br>`uint8_t presence` (0/1)<br>`float distance_m`<br>`float activity_level` |

---

## 4. Vibration Service
**Service UUID:** `12343000-...`
Handles data and configuration specifically for Mode 3.

| Characteristic | UUID | Properties | Data Type (C struct / Size) | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Vibration Config**| `...3001` | Read, Write | `struct` (~48 bytes) | A packed struct of all 16 `acc_vibration_config_t` parameters. |
| **Vibration Data** | `...3002` | Notify | `struct` (21 bytes) | `float max_amplitude`<br>`float time_series_std`<br>`float dom_frequency`<br>`float dom_displacement`<br>`uint8_t peak_count` |
| **Spectrum Array** | `...3003` | Notify | `float array` (TBD bytes) | Array of displacement values across frequency bins to draw the live spectrum graph. |

---

## How the Mobile App interacts with this Profile (Workflow)

### Standard Workflow (Starting a Measurement)
1. **Connection:** Phone connects to STM32 and subscribes to `Notify` on **Sensor Status** and the relevant **Data** characteristic.
2. **Setup:** App writes `1` to **Active Mode** (Selecting Distance/Vital).
3. **Configure:** App writes a byte array to **Distance Config**. The STM32 applies it and updates **Sensor Status** to `Prepared (2)`.
4. **Start:** App writes `1` to **System Command**.
5. **Stream:** STM32 updates **Sensor Status** to `Measuring (3)` and starts blasting notifications out of **Vital Data**.
6. **Stop:** App writes `0` to **System Command**. STM32 stops, calls `cleanup()`, and notifies **Sensor Status** `Powered Off (0)`.

### Mode Switching Workflow (e.g., Vital Sign to Vibration)
When the user taps the "Vibration" tab in the app while the sensor is running in Vital Sign mode:
1. **Stop Current Mode:** App writes `0` to **System Command**. The STM32 gracefully stops the distance detector, calls `cleanup()`, and goes into `Powered Off (0)`.
2. **Switch State:** App writes `3` to **Active Mode** (Selecting Vibration).
3. **Re-subscribe:** App unsubscribes from **Vital Data** notifications and subscribes to **Vibration Data** and **Spectrum Array** notifications.
4. **Configure:** App writes the packed struct to **Vibration Config**. The STM32 initializes the vibration detector resources and updates **Sensor Status** to `Prepared (2)`.
5. **Start:** App writes `1` to **System Command**. The STM32 begins vibrating measurements.

---

## 5. Edge Cases & FSM Considerations (For Future Implementation)
As the firmware develops into a full Finite State Machine (FSM), the following edge case workflows must be accounted for:

1. **Link Loss (Unexpected Disconnect):** If the BLE connection drops, the STM32 must intercept the disconnect event, automatically call `cleanup()`, return to `Powered Off` state, and restart BLE advertising to save battery.
2. **On-the-Fly Tuning:** To change a setting while measuring, the App must quickly send Stop (`0`), write the new Config, and send Start (`1`).
3. **Hardware Fault / Recovery:** If an SPI timeout or unrecoverable error occurs, STM32 calls `cleanup()`, sets status to `Error`, and waits for the App to acknowledge and reset.
4. **Low Battery Brownout:** If battery drops below a critical threshold (e.g., 1%), force `cleanup()` and disable BLE to prevent corruption.
