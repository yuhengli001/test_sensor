# A121 Sensor API Reference

This guide combines all manual configuration options for the A121 sensor.

## Table 1: Global Sensor Configuration (acc_config.h)
These settings apply to the overall frame or the first sweep.

| Parameter Name | Setter Function | Default | Min / Max |
| :--- | :--- | :--- | :--- |
| **start_point** | `acc_config_start_point_set` | 80 | - / - |
| **num_points** | `acc_config_num_points_set` | 160 | - / - |
| **step_length** | `acc_config_step_length_set` | 1 | 1 / - |
| **hwaas** | `acc_config_hwaas_set` | 8 | 1 / 511 |
| **receiver_gain** | `acc_config_receiver_gain_set` | 16 | 0 / 23 |
| **enable_tx** | `acc_config_enable_tx_set` | true | - |
| **phase_enhancement** | `acc_config_phase_enhancement_set` | false | - |
| **iq_imbalance** | `acc_config_iq_imbalance_compensation_set` | false | - |
| **enable_loopback** | `acc_config_enable_loopback_set` | false | - |
| **prf** | `acc_config_prf_set` | 15.6 MHz | 5.2 / 19.5 MHz |
| **profile** | `acc_config_profile_set` | PROFILE 3 | 1 / 5 |
| **sweep_rate** | `acc_config_sweep_rate_set` | 0.0 (Max) | - / - |
| **frame_rate** | `acc_config_frame_rate_set` | 0.0 (Max) | - / - |
| **sweeps_per_frame** | `acc_config_sweeps_per_frame_set` | 1 | - / - |
| **continuous_mode** | `acc_config_continuous_sweep_mode_set` | false | - |
| **double_buffering** | `acc_config_double_buffering_set` | false | - |
| **num_subsweeps** | `acc_config_num_subsweeps_set` | 1 | 1 / 4 |
| **inter_frame_idle** | `acc_config_inter_frame_idle_state_set`| deep_sleep| - |
| **inter_sweep_idle** | `acc_config_inter_sweep_idle_state_set`| ready | - |

---

## Table 2: Subsweep Configuration (acc_config_subsweep.h)
These settings are used when `num_subsweeps > 1`. Each function requires an **`index`** (0 to 3).

| Parameter Name | Subsweep Setter Function | Description |
| :--- | :--- | :--- |
| **start_point** | `acc_config_subsweep_start_point_set` | Start distance for this subsweep |
| **num_points** | `acc_config_subsweep_num_points_set` | Samples in this subsweep |
| **step_length** | `acc_config_subsweep_step_length_set` | Step size for this subsweep |
| **profile** | `acc_config_subsweep_profile_set` | Pulse profile for this subsweep |
| **hwaas** | `acc_config_subsweep_hwaas_set` | SNR averaging for this subsweep |
| **receiver_gain** | `acc_config_subsweep_receiver_gain_set` | Gain for this subsweep |
| **enable_tx** | `acc_config_subsweep_enable_tx_set` | Enable TX for this subsweep |
| **prf** | `acc_config_subsweep_prf_set` | PRF for this subsweep |
| **phase_enh** | `acc_config_subsweep_phase_enhancement_set` | Phase enh for this subsweep |
| **iq_imbalance** | `acc_config_subsweep_iq_imbalance_compensation_set` | IQ Comp for this subsweep |
| **loopback** | `acc_config_subsweep_enable_loopback_set` | Loopback for this subsweep |

---

### Key Definitions Reference
*   **Idle States**: `ACC_CONFIG_IDLE_STATE_DEEP_SLEEP`, `ACC_CONFIG_IDLE_STATE_SLEEP`, `ACC_CONFIG_IDLE_STATE_READY`
*   **Profiles**: `ACC_CONFIG_PROFILE_1` to `ACC_CONFIG_PROFILE_5`
*   **PRF**: `ACC_CONFIG_PRF_5_2_MHZ`, `6_5`, `8_7`, `13_0`, `15_6`, `19_5`
