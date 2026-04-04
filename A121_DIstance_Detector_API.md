# A121 Distance Detector API Reference

This outline provides a quick reference for all major functions and types in the Distance Detector API.

## Core Handle Types
- `acc_detector_distance_handle_t` - Main detector handle
- `acc_detector_distance_config_t` - Configuration handle
- `acc_detector_distance_result_t` - Result structure containing targets

## Operational Functions
- `acc_detector_distance_config_create()` - Initialize a new config
- `acc_detector_distance_config_destroy()` - Free constant config
- `acc_detector_distance_config_log()` - Log the current settings
- `acc_detector_distance_create()` - Create the operational handle
- `acc_detector_distance_destroy()` - Free operational resources
- `acc_detector_distance_get_sizes()` - Calculate buffer requirements
- `acc_detector_distance_calibrate()` - Perform initial calibration
- `acc_detector_distance_update_calibration()` - Update calibration (temperature change)
- `acc_detector_distance_prepare()` - Load config into sensor RAM
- `acc_detector_distance_process()` - Process raw data into distances

## Configuration (Setters & Getters)
### Range Settings
- `acc_detector_distance_config_start_set` / `get` (m)
- `acc_detector_distance_config_end_set` / `get` (m)

### Performance & Quality
- `acc_detector_distance_config_max_step_length_set` / `get`
- `acc_detector_distance_config_signal_quality_set` / `get`
- `acc_detector_distance_config_max_profile_set` / `get`
- `acc_detector_distance_config_reflector_shape_set` / `get`

### Threshold & Sorting
- `acc_detector_distance_config_threshold_method_set` / `get`
- `acc_detector_distance_config_threshold_sensitivity_set` / `get`
- `acc_detector_distance_config_peak_sorting_set` / `get`

### Calibration Settings
- `acc_detector_distance_config_num_frames_recorded_threshold_set` / `get`
- `acc_detector_distance_config_close_range_leakage_cancellation_set` / `get`
