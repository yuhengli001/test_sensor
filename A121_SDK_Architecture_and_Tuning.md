# A121 SDK Architecture & Tuning Guide

This guide provides a deep dive into how the Acconeer A121 Radar SDK is structured and how to effectively tune it for your application.

## 1. System Architecture Layers

The SDK is built in layers, moving from raw hardware access to high-level object detection:

| Layer | Component | Description |
| :--- | :--- | :--- |
| **User App** | `example_detector_distance.c` | Your code that uses the results (e.g., "Target at 1.25m"). |
| **Detector** | `acc_detector_distance.h` | **High-Level API**. Mathematical algorithms (CFAR, peak sorting) that turn raw data into targets. |
| **Service** | `acc_config.h` / `acc_processing.h` | **Low-Level API**. Configures the physical radar pulse and returns raw IQ (complex) data. |
| **RSS (Core)** | `acc_rss_a121.h` | The Radar System Software. Manages the sensor state, memory, and SPI communication. |
| **HAL** | `acc_hal_integration_a121.h` | The Hardware Abstraction Layer. This connects the RSS to your specific STM32 hardware (GPIOs, SPI, Timers). |

---

## 2. The Relationship: Services vs. Detectors

### The Service (Low-Level)
When you use a Service (like the Sparse IQ service), you are manually driving the radar. You get a "Point Cloud" of reflections, but you have to write your own code to decide if a reflection is a person, a wall, or just noise.

### The Detector (High-Level)
The Distance Detector **automates the Service**. When you tell the detector you want "High Signal Quality," it internally calculates the correct HWAAS, Pulse Profile, and Gain settings for the Service to achieve that goal.

> [!TIP]
> **Rule of Thumb:** If you want to find objects or measure distance, use the **Detector**. If you want to do research (like measuring material density or micro-vibrations), use the **Service**.

---

## 3. Key Tuning Parameters (The "Knobs")

When tuning the radar, you are navigating the trade-off between **Accuracy**, **Detection Range**, and **Power Consumption**.

### HWAAS (Hardware Averaging)
*   **Physical Effect**: The sensor takes multiple samples and averages them in hardware before sending them to the MCU.
*   **Increase it**: Better SNR (less noise), but takes longer to measure (higher power).
*   **Decrease it**: Faster updates and lower power, but the data will be noisier.

### Pulse Profiles (PROFILE_1 to PROFILE_5)
*   **Profile 1 (Narrow Pulse)**: Best **Resolution**. Can distinguish two objects very close together. Shortest range (~2 meters).
*   **Profile 3 (Medium Pulse)**: Balanced performance. Default for most applications.
*   **Profile 5 (Wide Pulse)**: Best **Sensitivity**. Can see objects very far away (up to 20m), but cannot distinguish between two objects that are close to each other.

### Pulse Repetition Frequency (PRF)
*   Controls how fast pulses are sent.
*   **High PRF**: Faster measurements, but limits the Maximum Measurable Distance (MMD).
*   **Low PRF**: Allows for very long-range detection but takes more time per frame.

---

## 4. The Critical Role of Calibration

Radar is extremely sensitive to its environment. Calibration is not optional; it is required for reliable performance.

1.  **Sensor Calibration**: Compensates for internal electronics and temperature.
2.  **Detector Calibration**: Learns the "Static Environment." 
    *   **Close Range Leakage**: If the sensor is behind a cover (radome), calibration "learns" the reflection from that cover so it can subtract it and "see through" the plastic.
    *   **Background Noise**: Establishes the "noise floor" so the detector knows that a tiny reflection at 5 meters is just noise, not a target.

> [!IMPORTANT]
> If the temperature changes significantly (typically >15°C), you **must re-calibrate**. The `acc_detector_distance_process` function will alert you to this by setting the `calibration_needed` flag in the result.
