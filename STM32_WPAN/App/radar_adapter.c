#include "radar_adapter.h"

#include "acc_hal_definitions_a121.h"
#include "acc_hal_integration_a121.h"
#include "acc_rss_a121.h"
#include "acc_integration.h"
#include "acc_sensor.h"
#include "acc_processing.h"
#include "acc_config.h"
#include "log_module.h"

/* Vibration Example Includes */
#include "example_vibration.h"
#include "vibration_service_app.h"

/* Presence (Vital/Fall) Includes */
#include "acc_detector_presence.h"
#include "fall_detector.h"
#include "vital_signs.h"
#include <complex.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define SENSOR_ID (1U)
#define SENSOR_TIMEOUT_MS (1000U)

/* --- Common State --- */
static acc_sensor_t             *sensor      = NULL;
static void                     *buffer      = NULL;
static uint32_t                  buffer_size = 0U;
static acc_processing_t         *processing  = NULL;
static acc_processing_metadata_t proc_meta   = {0};

/* --- Vibration State --- */
static acc_vibration_handle_t   *vib_handle  = NULL;
static acc_vibration_config_t    vib_config  = {0};

/* --- Presence (Vital/Fall) State --- */
static acc_detector_presence_handle_t *presence_handle = NULL;
static acc_detector_presence_config_t *presence_config = NULL;
static uint16_t presence_num_points = 0;

static float prev_angle = 0.0f;
static float unwrapped_angle = 0.0f;
static bool first_phase = true;
static float previous_presence_dist = 0.0f;
static bool has_previous_presence = false;
static uint32_t missing_target_count = 0;
static float last_known_dist = 0.0f;
static float ema_dist = 0.0f;

/* Forward declaration for calibration */
static bool do_sensor_calibration_and_prepare(acc_sensor_t *sensor, void *buffer, uint32_t buffer_size, const acc_config_t *sensor_config);

bool Radar_Adapter_Init(void) {
    const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();
    if (!acc_rss_hal_register(hal)) {
        LOG_INFO_APP("Radar HAL init failed\r\n");
        return false;
    }
    LOG_INFO_APP("Radar Adapter Initialized\r\n");
    return true;
}

static bool init_vibration(void) {
    acc_vibration_preset_set(&vib_config, ACC_VIBRATION_PRESET_LOW_FREQUENCY);

    vib_handle = acc_vibration_handle_create(&vib_config);
    if (!vib_handle) return false;

    processing = acc_processing_create(acc_vibration_handle_sensor_config_get(vib_handle), &proc_meta);
    if (!processing) return false;

    acc_rss_get_buffer_size(acc_vibration_handle_sensor_config_get(vib_handle), &buffer_size);
    buffer = acc_integration_mem_alloc(buffer_size);
    if (!buffer) return false;

    acc_hal_integration_sensor_supply_on(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    sensor = acc_sensor_create(SENSOR_ID);
    if (!sensor) return false;

    if (!do_sensor_calibration_and_prepare(sensor, buffer, buffer_size, acc_vibration_handle_sensor_config_get(vib_handle))) {
        return false;
    }

    return true;
}

static bool init_presence(void) {
    presence_config = acc_detector_presence_config_create();
    if (!presence_config) return false;

    acc_detector_presence_config_start_set(presence_config, 0.3f);
    acc_detector_presence_config_end_set(presence_config, 2.5f);
    acc_detector_presence_config_automatic_subsweeps_set(presence_config, true);
    acc_detector_presence_config_signal_quality_set(presence_config, 20.0f);
    acc_detector_presence_config_sweeps_per_frame_set(presence_config, 16);
    acc_detector_presence_config_frame_rate_set(presence_config, 20.0f); // SAMPLE_RATE_HZ
    acc_detector_presence_config_frame_rate_app_driven_set(presence_config, false);
    acc_detector_presence_config_reset_filters_on_prepare_set(presence_config, true);
    acc_detector_presence_config_intra_detection_set(presence_config, true);
    acc_detector_presence_config_intra_detection_threshold_set(presence_config, 1.3f);
    acc_detector_presence_config_inter_detection_set(presence_config, true);
    acc_detector_presence_config_inter_detection_threshold_set(presence_config, 1.0f);

    acc_detector_presence_metadata_t metadata;
    presence_handle = acc_detector_presence_create(presence_config, &metadata);
    if (!presence_handle) return false;
    presence_num_points = metadata.num_points;

    if (!acc_detector_presence_get_buffer_size(presence_handle, &buffer_size)) return false;
    
    buffer = acc_integration_mem_alloc(buffer_size);
    if (!buffer) return false;

    acc_hal_integration_sensor_supply_on(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    sensor = acc_sensor_create(SENSOR_ID);
    if (!sensor) return false;

    acc_cal_result_t cal_result;
    bool cal_complete = false;
    bool status = false;
    do {
        status = acc_sensor_calibrate(sensor, &cal_complete, &cal_result, buffer, buffer_size);
        if (status && !cal_complete) acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
    } while (status && !cal_complete);

    if (!status) return false;

    if (!acc_detector_presence_prepare(presence_handle, presence_config, sensor, &cal_result, buffer, buffer_size)) return false;

    fall_detector_init();
    vital_signs_init();
    first_phase = true;
    unwrapped_angle = 0.0f;
    has_previous_presence = false;
    missing_target_count = 0;
    last_known_dist = 0.0f;

    return true;
}

bool Radar_Adapter_Start(Radar_Mode_t mode) {
    // 1. Memory Cleanup: Stop any existing mode before starting a new one
    Radar_Adapter_Stop();

    // 2. Start specific mode
    if (mode == RADAR_MODE_VIBRATION) {
        LOG_INFO_APP("Radar Adapter: Starting VIBRATION mode\r\n");
        return init_vibration();
    }
    else if (mode == RADAR_MODE_VITAL) {
        LOG_INFO_APP("Radar Adapter: Starting VITAL mode\r\n");
        return init_presence();
    }
    else if (mode == RADAR_MODE_FALL) {
        LOG_INFO_APP("Radar Adapter: Starting FALL mode\r\n");
        return init_presence();
    }
    
    return false;
}

bool Radar_Adapter_Process(Radar_Mode_t mode) {
    if (!sensor) return false;

    if (!acc_sensor_measure(sensor)) return false;
    
    if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS)) return false;

    if (!acc_sensor_read(sensor, buffer, buffer_size)) return false;

    if (mode == RADAR_MODE_VIBRATION) {
        acc_processing_result_t proc_result = {0};
        acc_processing_execute(processing, buffer, &proc_result);

        if (proc_result.calibration_needed) {
            LOG_INFO_APP("Radar needs re-calibration.\r\n");
            const acc_config_t *cfg = acc_vibration_handle_sensor_config_get(vib_handle);
            if (cfg) do_sensor_calibration_and_prepare(sensor, buffer, buffer_size, cfg);
            return false; // Skip this frame
        }

        if (vib_handle) {
            acc_vibration_result_t result = {0};
            acc_vibration_process(&proc_result, vib_handle, &vib_config, &result);
            
            float top_freq = 0;
            float top_disp = 0;
            float top_vel = 0;
            float top_accel = 0;

            if (result.peak_count > 0) {
                top_disp = result.peak_displacements[0];
                top_freq = result.peak_frequencies[0];
                float angular_frequency = 2.0f * M_PI * top_freq;
                top_vel = (top_disp * angular_frequency) / 1e3f;
                top_accel = (top_disp * (angular_frequency * angular_frequency)) / 1e6f;
            }

            VIBRATION_APP_UpdateData(top_freq, top_disp, top_vel, top_accel);
        }
    } else if (mode == RADAR_MODE_VITAL || mode == RADAR_MODE_FALL) {
        if (!presence_handle) return false;
        
        acc_detector_presence_result_t result;
        if (!acc_detector_presence_process(presence_handle, buffer, &result)) return false;

        if (result.processing_result.calibration_needed) {
            LOG_INFO_APP("Sensor recalibration needed ... \r\n");
            acc_cal_result_t cal_result;
            bool cal_complete = false;
            bool status = false;
            do {
                status = acc_sensor_calibrate(sensor, &cal_complete, &cal_result, buffer, buffer_size);
                if (status && !cal_complete) acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
            } while (status && !cal_complete);
            if (status) acc_detector_presence_prepare(presence_handle, presence_config, sensor, &cal_result, buffer, buffer_size);
            return false;
        }

        if (result.presence_detected) {
            last_known_dist = result.presence_distance;
            missing_target_count = 0;
        } else {
            missing_target_count++;
        }

        if (last_known_dist > 0.1f && missing_target_count < 40) { // 2 seconds @ 20Hz
            float current_dist = last_known_dist;
            
            if (!has_previous_presence) {
                ema_dist = current_dist;
                previous_presence_dist = current_dist;
                has_previous_presence = true;
            } else {
                ema_dist = 0.15f * current_dist + 0.85f * ema_dist;
                float dist_diff = ema_dist - previous_presence_dist;
                float velocity = dist_diff * 20.0f; // SAMPLE_RATE_HZ
                if (mode == RADAR_MODE_FALL || mode == RADAR_MODE_VITAL) {
                    process_fall_detection(velocity, dist_diff, ema_dist);
                }
                previous_presence_dist = ema_dist;
            }

            int index = 0;
            float max_score = 0.0f;
            for (uint32_t i = 0; i < result.depthwise_presence_scores_length; i++) {
                if (result.depthwise_inter_presence_scores[i] > max_score) {
                    max_score = result.depthwise_inter_presence_scores[i];
                    index = (int)i;
                }
            }

            if (index >= 0 && index < presence_num_points) {
                float complex mean_sweep = 0.0f + 0.0f * I;
                uint16_t sweeps_per_frame = acc_detector_presence_config_sweeps_per_frame_get(presence_config);
                for (int s = 0; s < sweeps_per_frame; s++) {
                    acc_int16_complex_t point = result.processing_result.frame[s * presence_num_points + index];
                    mean_sweep += (float)point.real + (float)point.imag * I;
                }
                mean_sweep /= sweeps_per_frame;

                float angle = cargf(mean_sweep);

                if (first_phase) {
                    prev_angle = angle;
                    unwrapped_angle = angle;
                    first_phase = false;
                } else {
                    float angle_diff = angle - prev_angle;
                    if (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
                    else if (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;
                    unwrapped_angle += angle_diff;
                    prev_angle = angle;
                }

                if (mode == RADAR_MODE_VITAL || mode == RADAR_MODE_FALL) {
                    process_vital_signs(unwrapped_angle, current_dist);
                }
            }
        } else if (missing_target_count >= 40) {
            has_previous_presence = false;
            first_phase = true;
            last_known_dist = 0.0f;
        }
    }
    
    return true;
}

void Radar_Adapter_Stop(void) {
    LOG_INFO_APP("Radar Adapter: Stopping and cleaning memory...\r\n");
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_supply_off(SENSOR_ID);

    if (sensor) { acc_sensor_destroy(sensor); sensor = NULL; }
    if (processing) { acc_processing_destroy(processing); processing = NULL; }
    if (buffer) { acc_integration_mem_free(buffer); buffer = NULL; }
    
    /* --- Mode-Specific Handle Destruction --- */
    if (vib_handle) { acc_vibration_handle_destroy(vib_handle); vib_handle = NULL; }
    
    if (presence_handle) { acc_detector_presence_destroy(presence_handle); presence_handle = NULL; }
    if (presence_config) { acc_detector_presence_config_destroy(presence_config); presence_config = NULL; }
}

static bool do_sensor_calibration_and_prepare(acc_sensor_t *s, void *b, uint32_t b_size, const acc_config_t *s_config) {
    bool status = false;
    bool cal_complete = false;
    acc_cal_result_t cal_result = {0};

    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    do {
        status = acc_sensor_calibrate(s, &cal_complete, &cal_result, b, b_size);
        if (status && !cal_complete) {
            status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
        }
    } while (status && !cal_complete);

    if (status) {
        acc_hal_integration_sensor_disable(SENSOR_ID);
        acc_hal_integration_sensor_enable(SENSOR_ID);
        status = acc_sensor_prepare(s, s_config, &cal_result, b, b_size);
    }

    return status;
}
