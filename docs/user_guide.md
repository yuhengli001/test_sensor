# Radar Sensor — User Guide

---

## Table of Contents

1. [What Is This Device?](#1-what-is-this-device)
2. [Getting Started](#2-getting-started)
   - [Power On](#power-on)
   - [Install the App](#install-the-app)
   - [First-Time Connection](#first-time-connection)
3. [Vibration Monitoring](#3-vibration-monitoring)
   - [Setting Up](#setting-up)
   - [Sensor Settings](#sensor-settings)
   - [Reading the Results](#reading-the-results)
   - [Starting and Stopping](#starting-and-stopping)
4. [Vital Signs Monitoring](#4-vital-signs-monitoring)
   - [Setting Up](#setting-up-1)
   - [Reading the Results](#reading-the-results-1)
   - [Tips for Better Readings](#tips-for-better-readings)
5. [Fall Detection](#5-fall-detection)
   - [Setting Up](#setting-up-2)
   - [How It Works](#how-it-works)
   - [Detection Behaviour](#detection-behaviour)
   - [Alarm and Reset](#alarm-and-reset)
   - [Tips for Better Results](#tips-for-better-results)
6. [Tips and Best Practices](#6-tips-and-best-practices)
   - [Vibration Measurements](#vibration-measurements)
   - [Vital Signs Measurements](#vital-signs-measurements)
   - [General](#general)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. What Is This Device?

This project is a **complete radar sensing platform** — hardware, firmware, and companion app — designed so that developers can take it as a foundation and integrate it into their own specific applications with minimal rework.

The platform is built around the **Acconeer A121**, a 60 GHz mmWave pulsed coherent radar sensor. At just 29 mm² and 0.88 mm tall, it is one of the smallest radar sensors available, while simultaneously setting a new benchmark for low power consumption — making it uniquely suited for battery-powered, embedded applications. mmWave radar is a powerful sensing technology capable of detecting presence, motion, distance, vibration, vital signs, material classification, gesture recognition, and more — all without any physical contact. The two demonstration applications shown in this guide represent only a small slice of what the sensor can do.

**Demonstration applications in this release:**

| Mode | What you will see |
|---|---|
| Vibration | Live frequency (Hz) and displacement (µm) of a vibrating surface |
| Vital Signs | Breathing rate and heart rate, measured from a distance |
| Fall Detection | *(Coming soon)* |

The sensor runs on a built-in rechargeable battery and charges over USB. The BLE interface, firmware architecture, and iOS app are all structured to make it straightforward to add new sensing modes or swap in custom signal processing.

<table>
  <tr>
    <td align="center"><img src="images/IMG_6297.PNG" width="400"/><br><em>(front)</em></td>
    <td align="center"><img src="images/IMG_6298.PNG" width="400"/><br><em>Radar sensor (back)</em></td>
    <td align="center"><img src="images/case.jpg" width="400"/><br><em>(3d printing case and lens)</em></td>
  </tr>
</table>

---

## 2. Getting Started

### Power On

The sensor has a slide switch to turn the system on or off, though it is typically left on. Once powered, the hardware boots automatically and starts advertising over Bluetooth.

<!-- ### Install the App

Install **RadarPro** on your iPhone. The app requires iOS 16 or later and Bluetooth to be enabled. -->

### First-Time Connection

1. Open the app and go to the **Vibration** or **Vital Signs** tab.
2. Tap **CONNECT** in the top-right corner.
3. The app scans automatically and connects to the first sensor it finds.
4. The button changes to **DISCONNECT** once connected — you are ready.

<!-- > Keep the sensor within about 5 metres of your phone during setup. Once connected, the Bluetooth link is stable up to typical room distances. -->

---

## 3. Vibration Monitoring

Use this mode to measure mechanical vibrations on a surface — motors, pipes, machinery, structural elements, or anything that vibrates.

### Setting Up

1. Place the sensor so it faces the surface you want to measure and measure the distance between them.
2. Set the **Detection Range** slider in the app to match that distance before starting.
3. Tap the **Vibration** tab, connect if not already connected, then tap **START MONITOR**.
4. Once the sensor is running, **slowly adjust the position of the sensor or the object** — small shifts of a centimetre or two — until the frequency and displacement readings become stable and consistent.

> **Getting a stable reading is a positioning problem, not a settings problem.** The sensor analyses a single fixed point in space. If that point lands inside the object or just behind it rather than on the surface, the signal will be weak or absent. Small positional adjustments have a large effect — take your time here before changing any other settings.

### Sensor Settings

Before starting a measurement, you can adjust the settings in the **SENSOR SETTINGS** panel. Changes apply the next time you tap **START MONITOR**.

| Setting | What it does |
|---|---|
| **Frequency Mode** | **HIGH** — detects 10 to 5000 Hz vibrations (motors, machinery). **LOW** — detects 1 to 100 Hz vibrations (slow structural movement, low-speed equipment). |
| **Detection Range** | The distance from the sensor to the target surface. Slide to match your actual setup — from 0.1 m up to 1.0 m. The setting does not need to be exact — the sensor naturally detects within a window around the set distance (roughly ±5–10 cm with the default Pulse Profile 3). |
| **Pulse Profile** | Controls how much radar energy is sent out. Use higher values (4–5) for targets further away to get a stronger signal. |
| **HWAAS** | Controls how much internal averaging the radar does per measurement. Higher values improve signal quality at longer distances. Default (16) works well up to about 0.2 m. Use 64 or higher beyond 0.5 m. |
| **Continuous Sweep** | Keeps the sweep timing perfectly uniform — required for accurate frequency readings. Automatically ON in Low Frequency mode. |
| **Double Buffering** | Prevents any gaps in the data stream. Must be used together with Continuous Sweep. Automatically ON in Low Frequency mode. |

> **For targets beyond 0.5 m:** set HWAAS to 64 or higher, Pulse Profile to 4 or 5, and turn on both Continuous Sweep and Double Buffering together. These four settings work as a group to maintain signal quality at longer range.

### Reading the Results

| Reading | What it means |
|---|---|
| **Frequency (Hz)** | The dominant vibration frequency detected at your target distance. |
| **Displacement (µm)** | How far the surface is physically moving back and forth at that frequency, in micrometres (millionths of a metre). A higher number means stronger vibration amplitude. |

The display updates roughly 10 times per second. The readings are filtered — the app only shows a value once the measurement has been stable for a few consecutive frames, so you may see a short delay before the first reading appears.

If the display shows **0.0**, the sensor is not detecting a clear vibration at the current settings. Try adjusting the detection range to match your target distance more closely.

<table>
  <tr>
    <td align="center"><img src="images/1.PNG" width="200"/></td>
    <td align="center"><img src="images/2.PNG" width="200"/></td>
    <td align="center"><img src="images/3.PNG" width="200"/></td>
  </tr>
</table>

### Starting and Stopping

- Tap **START MONITOR** to begin measuring.
- Tap **STOP SENSOR** to stop. The sensor powers down its radar to save battery.
- Any settings change while the sensor is running takes effect only after a stop-and-restart.

---

## 4. Vital Signs Monitoring

Use this mode to measure breathing rate and heart rate from a distance — no contact required. The person being monitored should be seated or lying still, facing the sensor.

### Setting Up

1. Place the sensor on a stable surface, aimed at the person's chest at a distance of approximately 0.5 to 2.5 metres.
2. The person should remain as still as possible — avoid talking or large movements during measurement.
3. Tap the **Vitals** tab, connect, then tap **START MONITOR**.

### Reading the Results

| Reading | What it means |
|---|---|
| **Heart Rate (BPM)** | Beats per minute. The large number shown in the centre of the screen. |
| **Breathing Rate (BPM)** | Breaths per minute. |
| **Distance (m)** | The estimated distance to the person being monitored. |

<table>
  <tr>
    <td align="center"><img src="images/4.PNG" width="220"/></td>
  </tr>
</table>

> **This device is not a medical instrument.** The vital sign readings are for general monitoring and research purposes only. Do not use them for clinical diagnosis or medical decisions.

### Tips for Better Readings

- Reduce ambient movement in the room — the radar picks up any motion in its field of view.
- Keep the sensor aimed at the chest area rather than the head or legs.
- **During the first 6–8 seconds after tapping START**, the sensor runs an internal calibration sweep across all range bins to pick the best one. No readings appear during this window — this is normal.
- After the calibration sweep, allow a further **10–15 seconds** for the breathing and heart-rate estimates to stabilise before reading the values.
- If the heart rate shows **0** while breathing rate is visible, the heart signal is not yet above the confidence threshold. Allow more time, minimise body movement, and ensure the sensor is aimed at the chest rather than the head or abdomen.
- If both readings stay at 0 after 30 seconds, try adjusting the sensor angle or distance slightly.

---

## 5. Fall Detection

Use this mode to automatically detect if a monitored person falls. The sensor watches for a sudden, sustained impact burst, then confirms whether the person has moved to a significantly different position — triggering an alarm if they remain there for the confirmation period.

### Setting Up

1. Mount the sensor at a **fixed position** — on a shelf, table, or wall bracket — with a clear view of the area where the person will be moving. A height of 0.5 to 2.0 m and a slight downward angle works well; avoid mounting directly above the person.
2. The person can move freely during normal activity. The sensor only triggers when it detects a sustained impact followed by a large position change.
3. Tap the **Fall Detection** tab, connect if not already connected, then tap **START MONITOR**.

### How It Works

The detector runs a four-stage process every frame:

| Stage | What it means |
|---|---|
| **Monitoring** | Normal; no event detected. |
| **Impact** | A sustained energy burst is accumulating — tracking to see if it is a real impact. |
| **Suspected** | Impact confirmed; the sensor is now checking whether the person has stayed at a displaced position. |
| **ALARM** | Fall confirmed — the person has been at a displaced position for the full confirmation window. |

### Detection Behaviour

**Impact detection:** The algorithm triggers when the radar's motion-energy score exceeds the impact threshold continuously for at least 0.5 seconds. A brief spike — dropping an object, a chair shifting — does not trigger it; the burst must be sustained.

**Confirmation window:** After an impact, the sensor checks whether the person has moved to a significantly different distance and stayed there. The person does **not** need to be still — struggling or movement at the fallen position counts. The fall is confirmed after the person remains displaced for the full confirmation period (default: 5 seconds).

**Automatic cancellation:** The suspected state is cancelled automatically if any of these occur:
- The person returns to their original position for 2 consecutive seconds — they recovered on their own.
- Very strong, sustained motion is detected for 1 second — the person is clearly standing up and walking away.
- 30 seconds pass without a clear result — safety timeout to avoid being stuck indefinitely.

**Resting-before-impact protection:** If the person was already still for more than 5 seconds before the impact (for example, lying in bed), the sensor requires a *larger* position change to confirm a fall. This reduces false alarms from bed rolls or repositioning movements.

### Alarm and Reset

When a fall is confirmed, the app displays an alarm screen and the sensor continuously signals the alarm state. Tap **CLEAR ALARM** in the app to acknowledge and return the sensor to normal monitoring.

> **This device is not a substitute for a professional emergency response system.** Do not rely on it as the sole safety measure for a person at high risk of falling.

### Tips for Better Results

- Ensure a clear, unobstructed line of sight between the sensor and the area being monitored.
- Avoid placing the sensor where everyday activities — sitting down quickly, bending to pick something up — would consistently score above the impact threshold. If false triggers occur, the threshold can be adjusted in firmware.
- For best distance tracking after impact, the sensor should be roughly at the same height as the person's standing centre of mass (~0.5 to 1.5 m), not mounted directly overhead.

---

## 6. Tips and Best Practices

### Vibration Measurements

- **Match the detection range to your actual target distance.** The slider in the settings panel sets the exact point the radar analyses — if it does not match where your target is, the signal will be weak or absent.
- **Start with the default settings** (High Frequency, 0.2 m, HWAAS 16, Profile 3) and adjust from there.
- **Low Frequency mode is for slow vibrations only** (below 100 Hz). Do not use it for motors or machinery running above 100 Hz — the readings will be incorrect.
- **Avoid pointing the sensor at a glass surface** — radar reflections from glass can be inconsistent.

### Vital Signs Measurements

- The subject should breathe normally and avoid talking during the measurement.
- A clear line of sight between the sensor and the subject's chest produces the best results.
- Large metal objects between the sensor and the subject can interfere with the signal.

### General

- The sensor automatically powers off its radar between sessions to extend battery life. You will not see any radar activity until you tap START.
- If the BLE connection drops, tap CONNECT again — the sensor continues to advertise and is ready to reconnect immediately.

---

## 7. Troubleshooting

*(To be filled in)*

---