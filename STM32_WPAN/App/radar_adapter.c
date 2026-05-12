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

bool Radar_Adapter_Start(Radar_Mode_t mode) {
    // 1. Memory Cleanup: Stop any existing mode before starting a new one
    Radar_Adapter_Stop();

    // 2. Start specific mode
    if (mode == RADAR_MODE_VIBRATION) {
        LOG_INFO_APP("Radar Adapter: Starting VIBRATION mode\r\n");
        return init_vibration();
    }
    else if (mode == RADAR_MODE_VITAL) {
        LOG_INFO_APP("Radar Adapter: Starting VITAL mode (TODO)\r\n");
        return true;
    }
    else if (mode == RADAR_MODE_FALL) {
        LOG_INFO_APP("Radar Adapter: Starting FALL mode (TODO)\r\n");
        return true;
    }
    
    return false;
}

bool Radar_Adapter_Process(Radar_Mode_t mode) {
    if (!sensor) return false;

    if (!acc_sensor_measure(sensor)) return false;
    
    if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS)) return false;

    if (!acc_sensor_read(sensor, buffer, buffer_size)) return false;

    acc_processing_result_t proc_result = {0};
    acc_processing_execute(processing, buffer, &proc_result);

    if (proc_result.calibration_needed) {
        LOG_INFO_APP("Radar needs re-calibration.\r\n");
        const acc_config_t *cfg = NULL;
        if (mode == RADAR_MODE_VIBRATION) cfg = acc_vibration_handle_sensor_config_get(vib_handle);
        
        if (cfg) {
            do_sensor_calibration_and_prepare(sensor, buffer, buffer_size, cfg);
        }
        return false; // Skip this frame
    }

    /* Process Mode-Specific Math and trigger BLE updates */
    if (mode == RADAR_MODE_VIBRATION && vib_handle) {
        acc_vibration_result_t result = {0};
        acc_vibration_process(&proc_result, vib_handle, &vib_config, &result);
        
        float top_freq = 0;
        float top_disp = 0;
        float top_vel = 0;
        float top_accel = 0;

        if (result.peak_count > 0) {
            top_disp = result.peak_displacements[0];
            top_freq = result.peak_frequencies[0];
            float angular_frequency = 2.0f * 3.14159265f * top_freq;
            top_vel = (top_disp * angular_frequency) / 1e3f;
            top_accel = (top_disp * (angular_frequency * angular_frequency)) / 1e6f;
        }

        /* Forward to BLE Service */
        VIBRATION_APP_UpdateData(top_freq, top_disp, top_vel, top_accel);
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
    
    /* TODO: Add checks for future modes here */
    // if (vital_handle) { acc_vital_handle_destroy(vital_handle); vital_handle = NULL; }
    // if (fall_handle) { acc_fall_handle_destroy(fall_handle); fall_handle = NULL; }
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
