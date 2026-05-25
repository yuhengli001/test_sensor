# BLE Radar App Specification

## 1. Core Operating Modes
The app and sensor will operate in three distinct modes. The user can switch between them, which will reconfigure the radar sensor on the fly.

*   **Mode 1: Vital Sign Detection** (Monitoring breathing rate - *No Heart Rate*)
*   **Mode 2: Fall Detection** (Monitoring a room/person for sudden drops and presence)
*   **Mode 3: Vibration** (Monitoring machinery or surfaces for micro-vibrations)

---

## 2. Data to Send (Sensor -> Phone)
This is the data your STM32 will calculate and send over BLE to the app.

### Common Data (Always Available)
*   **Sensor Status Lifecycle (Mapped to Acconeer SDK):**
    *   `Powered Off`: Sensor supply is physically off (e.g., via user command or `cleanup()`).
    *   `Initialized`: HAL is registered (`acc_hal_rss_integration_get_implementation()`) and config is created, but sensor is not yet active.
    *   `Prepared`: Configuration is applied and `initialize_detector_resources(&resources)` has successfully completed (sensor is created, calibrated, and prepared).
    *   `Measuring`: Actively inside the `while (true)` loop, successfully executing `do_detector_get_next(...)`.
    *   `Recalibrating`: `result.calibration_needed` returned true, executing `do_sensor_calibration(...)`.
    *   `Error`: A critical function failed (e.g., `do_detector_get_next` returned false) or `cleanup(&resources)` was triggered.
*   **Current Active Mode:** Vital, Fall, or Vibration
*   **Battery Level:** 0-100% (if your device is battery powered)

### Vital Sign Mode (Distance Detector)
*   **Breathing Rate:** Breaths Per Minute (RPM) or equivalent distance displacement frequency.
*   **Distance/Range:** Measured distance to the person.
*   **Signal Quality/Confidence:** 0-100% (to tell the user if the reading is accurate)
*   **Waveform Data:** (Optional) Real-time displacement data to draw a live breathing graph on the screen.

### Fall Detection Mode
*   **Fall Event Alert:** Boolean (True if a fall is detected). **(CRITICAL TRIGGER)**
*   **Presence Status:** Is someone in the room? (True/False).
*   **Distance/Range:** Distance to the tracked person.
*   **Activity Level:** (Optional) Continuous motion score, depending on the algorithm implementation.

### Vibration Mode
*   **Vibration Metrics Table:** A collection of values for the detected vibration peaks:
    *   **Frequency (Hz):** The frequency of the vibration peak.
    *   **Displacement (μm):** The displacement magnitude at that frequency.
    *   **Velocity (mm/s):** Calculated velocity of the vibration.
    *   **Acceleration (m/s²):** Calculated acceleration of the vibration.
*   **Freq vs Displacement Data:** The full spectrum array (FFT result) used to plot the Frequency vs. Displacement graph.
*   **Max Sweep Amplitude:** The raw amplitude measured.
*   **Vibration Alert:** Triggered if displacement or acceleration exceeds a threshold.

---

## 3. Commands to Control (Phone -> Sensor)
These are the instructions the user can send from the app to configure the sensor.

### General Controls
*   **Set Operating Mode:** Switch between Vital, Fall, and Vibration.
*   **Start/Stop:** Manually start or pause the radar measurements to save power.

### Mode-Specific Configuration (App to Sensor)

#### Mode 1: Vital Sign (Distance Detector) Parameters

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
*(To be determined later)*

#### Mode 3: Vibration Parameters

**User-Controllable (Main UI):**
*   `preset` (enum): Frequency range preset — **High Frequency** (10–5000 Hz, burst mode) or **Low Frequency** (1–100 Hz, continuous mode). Selecting a preset loads all underlying sensor parameters automatically (see locked values below).
*   `measured_point` (int): Distance point index to monitor (default 80). Corresponds to ~`measured_point × 2.5 mm` from the sensor. Maximum value is **400** (= 1000 mm / 1 m).
*   `displacement_mode` (enum): **Peak-to-Peak** (total travel, recommended — standard in ISO 10816) or **Amplitude** (raw FFT peak, single-sided).

**Advanced Settings (Gear Menu — Range & SNR Optimisation):**

These four parameters work as a **coordinated group**. They are most relevant when `measured_point` exceeds ~200 (≥ 500 mm), where signal strength drops significantly due to the radar R⁴ power law — doubling the distance produces a 16× weaker return signal. Enable them together for reliable detection up to 1 m.

---

*   `hwaas` (uint16_t): **Hardware Accelerated Average Samples** — default 16, range 1–511.

    The sensor hardware averages this many consecutive sweeps in silicon before passing a single result to the CPU. This is the most direct SNR improvement available: **each doubling of HWAAS yields approximately +3 dB SNR**, recovering signal that would otherwise be buried in noise. Unlike software averaging, it has minimal impact on power or latency because it happens inside the radar chip itself.

    | Distance (measured_point) | Recommended HWAAS |
    |---|---|
    | ≤ 200 mm (point ≤ 80) | 16 (default) |
    | ~500 mm (point ~200) | 32 |
    | ~750 mm (point ~300) | 48–64 |
    | ~1000 mm (point ~400) | 64 |

    **Trade-off:** Higher HWAAS slightly reduces the maximum achievable sweep rate. In practice this is negligible for vibration measurement.

---

*   `profile` (uint8_t): **Radar Pulse Profile** — default 3, range 1–5.

    Controls the duration of the transmitted radar pulse. A longer pulse puts more energy into each sweep, which significantly improves SNR at long range. Since vibration mode monitors a **single fixed point** rather than a range, the coarser spatial resolution of higher profiles has no practical downside here.

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
> The iOS app should enforce that **CSM and Double Buffering are always toggled together** (link their switches, or use a single "High-Performance Mode" toggle that sets both).

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

## 4. Mobile App Functions & UI Design
How the app will look and behave for the user.

*   **Navigation:** A simple Tab Bar at the bottom (Vital | Fall | Vibrate | Settings) to easily switch contexts.
*   **Real-time Visuals:**
    *   *Vital:* A calming, pulsating lungs animation with live graphs (no heart rate).
    *   *Fall:* A clear "Room Status" dashboard. Turns RED with a loud alert if a fall happens.
    *   *Vibration:*
        *   **Frequency vs. Displacement Graph:** A dynamic chart showing the vibration spectrum.
        *   **Metrics Table:** A clear table displaying Frequency (Hz), Displacement (μm), Velocity (mm/s), and Acceleration (m/s²).
        *   **Control Panel:** Expandable sections for "Sensor Parameters" and "App Parameters" to match the Exploration Tool interface.
*   **Emergency Contact Integration:** If a Fall Event Alert is received, the app will automatically make a phone call or send an SMS message to a pre-configured emergency contact.
*   **Push Notifications:** Send an alert to the user's phone if a Fall is detected or Vibration exceeds the threshold, even if the app is in the background.
*   **BLE Connection Manager:** A dedicated screen to scan for your STM32 device, connect, and view signal strength (RSSI).
