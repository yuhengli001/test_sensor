# BLE Radar App Specification

## 1. Core Operating Modes
The app and sensor will operate in three distinct modes. The user can switch between them, which will reconfigure the radar sensor on the fly.

*   **Mode 1: Vital Sign Detection** (Monitoring breathing rate and heart Rate)
*   **Mode 2: Fall Detection** (Monitoring a room/person for sudden drops and presence)
*   **Mode 3: Vibration** (Monitoring vibration)

---

## 2. Data to Send (Sensor -> Phone)
This is the data that STM32 will calculate and send over BLE to the app.

### Common Data (Always Available)
*   **Sensor Status Lifecycle (Mapped to Acconeer SDK):**
    > 📋 **Debug log only** — not displayed in the iOS app UI. States are printed via `LOG_INFO_APP` on the STM32 debug UART.
    *   `Powered Off`: Sensor supply is physically off (e.g., via user command or `cleanup()`).
    *   `Initialized`: HAL is registered (`acc_hal_rss_integration_get_implementation()`) and config is created, but sensor is not yet active.
    *   `Prepared`: Configuration is applied and `initialize_detector_resources(&resources)` has successfully completed (sensor is created, calibrated, and prepared).
    *   `Measuring`: Actively inside the `while (true)` loop, successfully executing `do_detector_get_next(...)`.
    *   `Recalibrating`: `result.calibration_needed` returned true, executing `do_sensor_calibration(...)`.
    *   `Error`: A critical function failed (e.g., `do_detector_get_next` returned false) or `cleanup(&resources)` was triggered.
*   **Current Active Mode:** Vital, Fall, or Vibration
<!-- *   **Battery Level:** Removed — charging IC does not expose SoC over a readable interface.
     Future option: estimate via STM32 ADC reading of a battery voltage divider,
     then map voltage → % using a LiPo discharge curve (~3.0 V = 0%, ~4.2 V = 100%).
     Accuracy would be ±10–15%, suitable only as a low-battery warning. -->

### Vital Sign Mode (Distance Detector)
*   **Breathing Rate:** Breaths Per Minute (RPM).
*   **Heart Rate:** Beats Per Minute (BPM).
*   **Distance/Range:** Measured distance to the person.
*   **Waveform Data:** (Optional) Real-time displacement data to draw a live breathing graph on the screen.
<!-- *   **Signal Quality/Confidence:** 0-100% (to tell the user if the reading is accurate) -->

### Fall Detection Mode
*   **Fall Event Alert:** Boolean (True if a fall is detected). **(CRITICAL TRIGGER)**
*   **Presence Status:** Is someone in the room? (True/False).
*   **Distance/Range:** Distance to the tracked person.
<!-- *   **Activity Level:** (Optional) Continuous motion score, depending on the algorithm implementation. -->

### Vibration Mode
*   **Frequency (Hz):** The frequency of the vibration peak.
*   **Displacement (μm):** The displacement magnitude at that frequency.
*   **Vibration configuration:** The parameters that can be adjusted by user. 
<!-- *   **Velocity (mm/s):** Calculated velocity of the vibration.
*   **Acceleration (m/s²):** Calculated acceleration of the vibration.
*   **Freq vs Displacement Data:** The full spectrum array (FFT result) used to plot the Frequency vs. Displacement graph.
*   **Max Sweep Amplitude:** The raw amplitude measured.
*   **Vibration Alert:** Triggered if displacement or acceleration exceeds a threshold. -->
<!-- *   **Vibration Metrics Table:** A collection of values for the detected vibration peaks: -->
---

## 3. Commands to Control (Phone -> Sensor)
These are the instructions the user can send from the app to configure the sensor.

### General Controls
*   **Set Operating Mode:** Switch between Vital, Fall, and Vibration.
*   **Start/Stop:** Manually start or stop the radar measurements to save power.

### Mode-Specific Configuration (App to Sensor)

#### Mode 1: Vital Sign (Distance Detector) Parameters

> ⚠️ **Under development** — not yet implemented in firmware or iOS app.

**Basic Settings (Main UI):**
*   `start_m` (float): Monitoring start distance in meters (default 0.25).
*   `end_m` (float): Monitoring end distance in meters (default 3.0).
*   `threshold_sensitivity` (float): Overall detection sensitivity (default 0.5).

**Advanced Settings (Hidden/Gear Menu):**
*   `max_step_length` (uint16_t): Maximum step length (default 0).
*   `max_profile` (enum): Max profile (default profile_5).
*   `signal_quality` (float): Signal quality threshold (default 15.0).
*   `threshold_method` (enum): CFAR or other threshold method (default cfar).
*   `peak_sorting_method` (enum): Peak sorting method (default strongest).
*   `reflector_shape` (enum): Reflector shape (default generic).
*   `num_frames_in_recorded_threshold` (uint16_t): Frames in recorded threshold (default 100).
*   `fixed_amplitude_threshold_value` (float): Fixed amplitude threshold (default 100.0).
*   `fixed_strength_threshold_value` (float): Fixed strength threshold (default 0.0).
*   `close_range_leakage_cancellation` (bool): Enable close range leakage cancellation (default false).

#### Mode 2: Fall Detection Parameters

> ⚠️ **Under development** — not yet implemented in firmware or iOS app.

#### Mode 3: Vibration Parameters

**User-Controllable (Main UI):**
*   `preset` (enum): Frequency range preset — **High Frequency** (100–5000 Hz) or **Low Frequency** (1–100 Hz). Selecting a preset loads all underlying sensor parameters automatically (see locked values below).
*   `measured_point` (int): Distance point index to monitor (default 80). Corresponds to ~`measured_point × 2.5 mm` from the sensor. Maximum value is **400** (= 1000 mm / 1 m).
<!-- *   `displacement_mode` (enum): **Peak-to-Peak** (total travel, recommended — standard in ISO 10816) or **Amplitude** (raw FFT peak, single-sided). -->

**Advanced Settings (Range & SNR Optimisation):**

These four parameters work as a **coordinated group**. They are most relevant when `measured_point` exceeds ~200 (≥ 500 mm), where signal strength drops significantly due to the radar R⁴ power law — doubling the distance produces a 16× weaker return signal. Enable them together for reliable detection up to 1 m.

---

*   `hwaas` (uint16_t): **Hardware Accelerated Average Samples** — default 16.

    Each radar sweep is one pulse fired and one echo received — a single raw sample that always contains random electronic noise. HWAAS tells the A121 chip to fire N pulses back-to-back internally and combine them into one averaged sweep result **before** passing anything to the STM32. Because the real reflection is consistent across all N pulses but noise is random, the noise partially cancels while the signal builds up. The STM32 just sees one clean sample, as if only one sweep happened. This is the most direct SNR improvement available: **each doubling of HWAAS yields approximately +3 dB SNR**. Unlike software averaging, it costs no CPU time because it happens entirely inside the radar chip's silicon.

    The firmware supports the full SDK range of 1–511, but the iOS app exposes **6 discrete levels** that cover all practical use cases:

    | Level | HWAAS | Recommended for |
    |---|---|---|
    | 1 | 8 | Very close range, fast response |
    | 2 | **16** *(default)* | ≤ 200 mm (point ≤ 80) |
    | 3 | 32 | ~500 mm (point ~200) |
    | 4 | 64 | ~750 mm (point ~300) |
    | 5 | 128 | ~1000 mm (point ~400) |
    | 6 | 256 | Noisy environments at long range |

    **Trade-off:** Higher HWAAS slightly reduces the maximum achievable sweep rate. In practice this is negligible for vibration measurement.

---

*   `profile` (uint8_t): **Radar Pulse Profile** — default 3, range 1–5.

    Controls the duration of the transmitted radar pulse. A longer pulse puts more energy into each sweep, which significantly improves SNR at long range.
     <!-- Since vibration mode monitors a **single fixed point** rather than a range, the coarser spatial resolution of higher profiles has no practical downside here. -->

    | Profile | Pulse Character | Best For |
    |---|---|---|
    | 1 | Shortest, finest resolution | Close range < 0.3 m, high clutter |
    | 2 | Short | 0.3–0.4 m |
    | 3 | Balanced *(default)* | General use up to ~0.5 m |
    | 4 | Long pulse, high SNR | 0.5–0.75 m |
    | 5 | Longest, best SNR | 0.75–1.0 m |

    **UI note:** Can be presented as a simple slider (1 → 5) labelled **"Pulse Strength"** to avoid exposing the technical term.

---

*   `continuous_sweep_mode` (bool): **Continuous Sweep Mode** — default OFF for High Frequency preset, ON for Low Frequency preset.

    When enabled, the sensor sweeps without any pause between sweeps. This guarantees **uniform time intervals** between data samples — a hard requirement for accurate FFT frequency analysis. Without CSM, small idle gaps appear between sweeps and introduce timing jitter that shifts and blurs vibration frequency peaks in the output spectrum, producing incorrect frequency readings.

    **Must always be paired with Double Buffering** when HWAAS > 16 or Profile ≥ 4. Enabling CSM alone without Double Buffering will cause the CPU to stall the sensor mid-stream, reintroducing the timing gaps CSM was meant to eliminate.

---

*   `double_buffering` (bool): **Double Buffering** — default OFF for High Frequency preset, ON for Low Frequency preset.

    Uses two alternating memory buffers. While the CPU reads completed frame data from **Buffer A**, the sensor continues writing new sweeps into **Buffer B**. When Buffer B fills, they swap. This prevents the CPU readout from stalling the sensor — which would otherwise create a timing gap in the sweep stream, defeating the purpose of Continuous Sweep Mode.

    **Always pair with Continuous Sweep Mode.** Enabling one without the other provides no benefit.

---

> **💡 Recommended configuration for 1 m detection:**
> `HWAAS = 64` · `Profile = 5` · `Continuous Sweep Mode = ON` · `Double Buffering = ON`
>
<!-- > The iOS app should enforce that **CSM and Double Buffering are always toggled together** (link their switches, or use a single "High-Performance Mode" toggle that sets both). -->

**Locked / Hardcoded on STM32 (not sent over BLE):**

| Parameter | High Frequency | Low Frequency | Notes |
|---|---|---|---|
| `sweep_rate` | 10000 Hz | 200 Hz | Sets max detectable freq (Nyquist = sweep_rate / 2). Both presets optimized for 0.2 m (point 80). |
| `sweeps_per_frame` | 1024 | 20 | Keeps frame rate ~10 fps for both presets |
| `lp_coeff` | 0.5 | 0.8 | More smoothing needed for slow low-freq signals |
| `low_frequency_enhancement` | false | true | Boosts weak low-freq FFT components |
| `amplitude_threshold` | 100.0 | 100.0 | Hardcoded — show diagnostic if signal too weak |
| `frame_rate` | 0 (unconstrained) | 0 | Runs as fast as hardware allows |
| `time_series_length` | 1024 | 1024 | freq_resolution = sweep_rate / time_series_length |
| `inter_frame_idle_state` | READY | READY | No power saving needed |
| `inter_sweep_idle_state` | READY | READY | No power saving needed |

---

## 4. Vibration Config Write Flow (iOS → MCU)

### When settings are applied

Settings changed in the UI are held in local app state only. They are **not sent to the MCU immediately** when the user moves a slider or picks a value. They are written to the sensor exactly once — at the moment the user taps **START MONITOR**.

### Start sequence (3 BLE writes in order)

```
User taps START
      │
      ├─ 1. Write mode byte → FE41 (Active Mode char, withoutResponse)
      │        value: 0x03 = Vibration
      │
      ├─ 2. Write config → FE71 (Vibration Config char, withResponse, 10 bytes)
      │        [preset | measuredPoint(4) | hwaas(2) | profile | csm | db]
      │
      └─ (200 ms delay)
           │
           └─ 3. Write start command → FE42 (System Command char, withoutResponse)
                    value: 0x01 = START
```

The 200 ms gap before the START command ensures both the mode byte and config bytes have been received and stored in the MCU before `Radar_Adapter_Start()` is called.

### MCU side (on receiving START command)

```
FE42 write (0x01) received
      │
      └─ Radar_Adapter_Start(RADAR_MODE_VIBRATION)
               │
               └─ init_vibration()
                        │
                        ├─ acc_vibration_preset_set()   ← loads Acconeer defaults
                        │
                        └─ apply user overrides from Vibration_Config
                                  measured_point   ← hard overwrite
                                  hwaas            ← hard overwrite
                                  profile          ← hard overwrite
                                  csm              ← OR'd (can only turn ON)
                                  double_buffering ← OR'd (can only turn ON)
```

### What happens when settings change while running

| Action | Effect |
|---|---|
| Move slider / change picker while running | Only local UI state changes — MCU is **not notified** |
| Tap **STOP SENSOR** | MCU stops; config is **not** sent |
| Tap **START MONITOR** again | Fresh config is sent, then START — new settings take effect |

**Rule of thumb:** any settings change requires a stop-and-restart cycle to take effect on the sensor.

### CSM and Double Buffering policy (preset-dependent)

These two flags are not simply passed through — they follow a preset-conditional rule in firmware:

```c
if (preset == LOW_FREQUENCY) {
    csm = true   // always forced ON — required for correct FFT timing
    db  = true   // always forced ON — required to pair with CSM
} else {         // HIGH_FREQUENCY
    csm = user_csm   // user has full control
    db  = user_db    // user has full control
}
```

- **LOW_FREQUENCY** — CSM and DB are always forced ON regardless of what the app sends. The iOS UI locks both toggles to ON (greyed out) when this preset is selected.
- **HIGH_FREQUENCY** — CSM and DB are fully user-controlled. They default OFF (short range), but should be enabled together for detection beyond ~500 mm.

---

## 5. Mobile App Functions & UI Design

### Navigation

Tab bar at the bottom with 3 tabs, sharing a single `RadarBLEManager` instance across all tabs so BLE state is preserved when switching:

| Tab | Icon | Status |
|---|---|---|
| Vibration | dot.radiowaves.left.and.right | ✅ Implemented |
| Vitals | heart.fill | ✅ Implemented |
| Fall | figure.fall | 🚧 Placeholder — "Algorithm Integration Pending" |

> 🔮 **Future update:** Add a dedicated Settings tab for BLE connection management and app-level preferences.

---

### BLE Connection

Each dashboard has its own **CONNECT / DISCONNECT** button in the top-right corner. Tapping CONNECT starts a BLE scan and auto-connects to the first peripheral whose name contains `"test_sensor"`. A scanning spinner is shown while searching. Disconnecting from one tab disconnects across all tabs since the BLE manager is shared.

> 🔮 **Future update:** Dedicated BLE scan screen showing all discovered peripherals, RSSI signal strength, and manual device selection.

---

### Vibration Dashboard

*   **Header:** "RADAR SENSOR / Vibration" title + CONNECT/DISCONNECT button
*   **Frequency display:** Large real-time readout of dominant vibration frequency (Hz)
*   **Displacement card:** Real-time displacement magnitude (µm)
*   **SENSOR SETTINGS panel** (always visible when connected, applies on next start):

    | Control | Type | Values |
    |---|---|---|
    | Frequency Mode | Menu picker | HIGH (10–5000 Hz) / LOW (0.1–100 Hz) |
    | Detection Range | Slider | 0.1 m – 1.0 m in 25 mm steps |
    | Pulse Profile | Segmented picker | 1 / 2 / 3 / 4 / 5 |
    | HWAAS | Menu picker | 8 / 16 / 32 / 64 / 128 / 256 |
    | Continuous Sweep | Toggle | ON/OFF — locked ON when LOW freq selected |
    | Double Buffering | Toggle | ON/OFF — locked ON when LOW freq selected |

*   **START MONITOR / STOP SENSOR** button pinned at the bottom

> 🔮 **Future update:** Frequency vs. Displacement spectrum graph (FFT chart). Velocity (mm/s) and Acceleration (m/s²) metric cards.

---

### Vital Signs Dashboard

*   **Header:** "RADAR SENSOR / Vital Signs" title + CONNECT/DISCONNECT button
*   **Heart rate display:** Large real-time readout (BPM) with a pulsing heart icon animation while running
*   **Secondary metrics:** Breathing Rate (BPM) and Distance (m) cards
*   **START MONITOR / STOP MONITOR** button

> 🔮 **Future update:** Live waveform graph showing real-time breathing displacement. Configurable monitoring range controls (start/end distance).

---

### Fall Detection Dashboard

> 🚧 **Placeholder** — shows "Algorithm Integration Pending". No live data or controls yet.

> 🔮 **Future update:** Room status display (presence detected / clear), fall event alert with red flash and audio, distance to tracked person. Emergency contact integration — automatic SMS/call on fall detection. Push notifications when app is in background.

---

### Shared Behaviour

*   Dark theme throughout (`#0D0D12` background)
*   All dashboards show an identical empty state ("No Radar Connected" + scan spinner) when BLE is disconnected
*   START command sequence: set mode → write config → 200 ms delay → send START (see Section 4)
