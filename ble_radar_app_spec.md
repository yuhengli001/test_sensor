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

**Basic Settings (Main UI):**
*   `measured_point` (int): The distance point index to monitor (default 80).
*   `amplitude_threshold` (int): Minimum signal strength to calculate vibration (default 100).
*   `threshold_margin_um` (float): Sensitivity margin in μm for peak detection (default 10.0).
*   `displacement_mode` (enum): Choose between "Amplitude" or "Peak-to-Peak".

**Advanced Settings (Hidden/Gear Menu):**
*   `profile` (int): Radar profile 1-5 (default 3).
*   `frame_rate_hz` (float): Frame rate in Hz (default 10.0).
*   `frame_rate_limit` (bool): Enable frame rate limit (default False).
*   `sweep_rate_hz` (float): Sweep rate in Hz (default 3000.0).
*   `sweeps_per_frame` (int): Number of sweeps per frame (default 128).
*   `hwaas` (int): Hardware averaging (default 16).
*   `double_buffering` (bool): Enable double buffering (default True).
*   `continuous_sweep_mode` (bool): Enable continuous sweep mode (default True).
*   `inter_frame_idle_state` (enum): Idle state between frames (Ready, Sleep, Deep Sleep).
*   `inter_sweep_idle_state` (enum): Idle state between sweeps (Ready, Sleep, Deep Sleep).
*   `time_series_length` (int): FFT calculation length (default 1024).
*   `time_filtering_coefficient` (float): Exponential filter coefficient (default 0.95).
*   `low_frequency_enhancement` (bool): Enable low frequency enhancement (default True).

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
