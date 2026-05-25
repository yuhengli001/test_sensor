# BLE GATT Profile Documentation

This document outlines the custom Bluetooth Low Energy (BLE) GATT services and characteristics implemented for the STM32WPAN Acconeer Radar Sensor project.

## 1. Control Service
**Service UUID:** `0000FE40-CC7A-482A-984A-7F2ED5B3E58F`

This service manages the global state and operating mode of the radar sensor.

| Characteristic | UUID | Properties | Description |
| :--- | :--- | :--- | :--- |
| **Active Mode** | `0000FE41-8E22-4541-9D4C-21EDAE82ED19` | Write | Set the active radar mode (0=None, 1=Vital, 2=Fall, 3=Vibration) |
| **System Command**| `0000FE42-8E22-4541-9D4C-21EDAE82ED19` | Write | Start (`0x01`) or Stop (`0x00`) the active radar task |
| **Sensor Status** | `0000FE43-8E22-4541-9D4C-21EDAE82ED19` | Notify / Read | Broadcasts the current status/health of the sensor |

---

## 2. Vital Sign Service
**Service UUID:** `0000FE50-CC7A-482A-984A-7F2ED5B3E58F`

This service handles configurations and data streams specific to Breathing and Heart Rate monitoring.

| Characteristic | UUID | Properties | Description |
| :--- | :--- | :--- | :--- |
| **Distance Config** | `0000FE51-8E22-4541-9D4C-21EDAE82ED19` | Write | Configuration parameters (e.g., target distance range) |
| **Vital Data** | `0000FE52-8E22-4541-9D4C-21EDAE82ED19` | Notify | Stream of calculated breathing/heart rates |
| **Waveform** | `0000FE53-8E22-4541-9D4C-21EDAE82ED19` | Notify | Raw or processed waveform data arrays |

---

## 3. Fall Detection Service
**Service UUID:** `0000FE60-CC7A-482A-984A-7F2ED5B3E58F`

This service is dedicated to detecting falls and abrupt changes in posture.

| Characteristic | UUID | Properties | Description |
| :--- | :--- | :--- | :--- |
| **Fall Config** | `0000FE61-8E22-4541-9D4C-21EDAE82ED19` | Write | Thresholds and sensitivity configuration |
| **Fall Data** | `0000FE62-8E22-4541-9D4C-21EDAE82ED19` | Notify | Alerts and tracking metrics when a fall is detected |

---

## 4. Vibration Service
**Service UUID:** `0000FE70-CC7A-482A-984A-7F2ED5B3E58F`

This service streams high-frequency micro-vibration data.

| Characteristic | UUID | Properties | Description |
| :--- | :--- | :--- | :--- |
| **Vibration Config**| `0000FE71-8E22-4541-9D4C-21EDAE82ED19` | Write | Configurations for vibration |
| **Vibration Data** | `0000FE72-8E22-4541-9D4C-21EDAE82ED19` | Notify | Frequency, Displacement |
| **Spectrum Array** | `0000FE73-8E22-4541-9D4C-21EDAE82ED19` | Notify | FFT Spectrum data array |

---

> [!TIP]
> All Characteristics share a common base UUID format: `XXXX-8E22-4541-9D4C-21EDAE82ED19`. The first 16 bits change to define the specific Characteristic ID (`0000FE41`, `0000FE42`, etc.).
