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

#define STABILITY_THRESHOLD 0.5f  /* Hz */
#define STABILITY_REQUIRED  3     /* Consecutive frames */
#define RADAR_FRAME_RATE    20.0f /* Hz */

typedef enum { DET_SEARCHING, DET_COARSE, DET_MEASURING } det_phase_t;

typedef struct {
    /* --- Common State --- */
    acc_sensor_t             *sensor;
    void                     *buffer;
    uint32_t                  buffer_size;
    acc_processing_t         *processing;
    acc_processing_metadata_t proc_meta;

    /* --- Vibration State --- */
    acc_vibration_handle_t   *vib_handle;
    acc_vibration_config_t    vib_config;
    float                     vib_prev_freq;
    uint32_t                  vib_stability_counter;
    bool                      vib_was_stable;

    /* --- Presence (Vital/Fall) State --- */
    acc_detector_presence_handle_t *presence_handle;
    acc_detector_presence_config_t *presence_config;
    uint16_t                  presence_num_points;

    float                     prev_angle;
    float                     unwrapped_angle;
    bool                      first_phase;
    float                     previous_presence_dist;
    bool                      has_previous_presence;
    uint32_t                  missing_target_count;
    float                     last_known_dist;
    float                     ema_dist;

    /* Coarse scan state machine */
    det_phase_t               det_phase;
    int                       tracked_index;
    uint32_t                  phase_renorm_counter;
    int                       n_candidates;
    int                       candidate_bins[COARSE_MAX_CANDS];
    float                     cand_prev_angle[COARSE_MAX_CANDS];
    float                     cand_unwrapped[COARSE_MAX_CANDS];
    bool                      cand_first_phase[COARSE_MAX_CANDS];
} Radar_Adapter_Context_t;

static Radar_Adapter_Context_t ctx = {0};

// Function declarations
static bool init_vibration(void);
static bool init_presence(void);
static bool do_sensor_calibration_and_prepare(acc_sensor_t *sensor, void *buffer, uint32_t buffer_size, const acc_config_t *sensor_config);

bool Radar_Adapter_Init(void);
bool Radar_Adapter_Start(Radar_Mode_t mode);
bool Radar_Adapter_Process(Radar_Mode_t mode);
void Radar_Adapter_Stop(void);


/***********************************************************************************************************************
 * @brief   Initialize the Radar RSS and HAL.
 * 
 * @return  true if the initialization was successful, false otherwise.
***********************************************************************************************************************/
bool Radar_Adapter_Init(void) {
    const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();
    if (!acc_rss_hal_register(hal)) {
        LOG_INFO_APP("Radar HAL init failed\r\n");
        return false;
    }
    LOG_INFO_APP("Radar Adapter Initialized\r\n");
    return true;
}

/***********************************************************************************************************************
 * @brief   Initialize the vibration mode.
 * 
 *          This initialization sequence performs the following steps:
 *          1. Sets the vibration configuration preset.
 *          2. Creates the vibration handle (`acc_vibration_handle_create`).
 *          3. Creates the processing instance (`acc_processing_create`).
 *          4. Allocates the required working buffer (`acc_integration_mem_alloc`).
 *          5. Turn on and enable the sensor (`acc_hal_integration_sensor_supply_on` and `acc_hal_integration_sensor_enable`).
 *          6. Create sensor (`acc_sensor_create`).
 *          7. Calibrates the sensor (`do_sensor_calibration_and_prepare`).
 *          
 * 
 * @return  true if the initialization was successful, false otherwise.
***********************************************************************************************************************/
static bool init_vibration(void) {
    // Get user configuration for vibration mode
    VIBRATION_Config_t *cfg = VIBRATION_APP_GetConfig();

    /* 1. Load preset (HIGH or LOW frequency) */
    acc_vibration_preset_t preset = (cfg->preset == 1)
        ? ACC_VIBRATION_PRESET_LOW_FREQUENCY
        : ACC_VIBRATION_PRESET_HIGH_FREQUENCY;
    acc_vibration_preset_set(&ctx.vib_config, preset);

    /* 2. Apply user overrides on top of preset */
    ctx.vib_config.measured_point          = cfg->measured_point;
    ctx.vib_config.hwaas                   = cfg->hwaas;
    ctx.vib_config.profile                 = (acc_config_profile_t)cfg->profile;
    /* OR with preset value: user can turn CSM/DB ON for extra performance,
     * but cannot accidentally turn them OFF when the preset requires them
     * (e.g. LOW_FREQUENCY preset needs both enabled for correct FFT timing). */
    ctx.vib_config.continuous_sweep_mode = ctx.vib_config.continuous_sweep_mode || (cfg->continuous_sweep_mode != 0);
    ctx.vib_config.double_buffering      = ctx.vib_config.double_buffering      || (cfg->double_buffering != 0);
    ctx.vib_config.reported_displacement_mode = ACC_VIBRATION_REPORT_DISPLACEMENT_AS_AMPLITUDE;

    LOG_INFO_APP("Vibration init: preset=%s, point=%lu, hwaas=%u, profile=%u, csm=%u, db=%u\r\n",
        (preset == ACC_VIBRATION_PRESET_HIGH_FREQUENCY) ? "HIGH" : "LOW",
        (unsigned long)cfg->measured_point,
        (unsigned)cfg->hwaas,
        (unsigned)cfg->profile,
        (unsigned)cfg->continuous_sweep_mode,
        (unsigned)cfg->double_buffering);

    ctx.vib_handle = acc_vibration_handle_create(&ctx.vib_config);
    if (!ctx.vib_handle) return false;

    ctx.processing = acc_processing_create(acc_vibration_handle_sensor_config_get(ctx.vib_handle), &ctx.proc_meta);
    if (!ctx.processing) return false;

    acc_rss_get_buffer_size(acc_vibration_handle_sensor_config_get(ctx.vib_handle), &ctx.buffer_size);
    ctx.buffer = acc_integration_mem_alloc(ctx.buffer_size);
    if (!ctx.buffer) return false;

    acc_hal_integration_sensor_supply_on(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    ctx.sensor = acc_sensor_create(SENSOR_ID);
    if (!ctx.sensor) return false;

    if (!do_sensor_calibration_and_prepare(ctx.sensor, ctx.buffer, ctx.buffer_size, acc_vibration_handle_sensor_config_get(ctx.vib_handle))) {
        return false;
    }

    return true;
}

static bool init_presence(void) {
    ctx.presence_config = acc_detector_presence_config_create();
    if (!ctx.presence_config) return false;

    acc_detector_presence_config_start_set(ctx.presence_config, 0.3f);
    acc_detector_presence_config_end_set(ctx.presence_config, 2.5f);
    acc_detector_presence_config_automatic_subsweeps_set(ctx.presence_config, true);
    acc_detector_presence_config_signal_quality_set(ctx.presence_config, 20.0f);
    acc_detector_presence_config_sweeps_per_frame_set(ctx.presence_config, 16);
    acc_detector_presence_config_frame_rate_set(ctx.presence_config, RADAR_FRAME_RATE); // SAMPLE_RATE_HZ
    acc_detector_presence_config_frame_rate_app_driven_set(ctx.presence_config, false);
    acc_detector_presence_config_reset_filters_on_prepare_set(ctx.presence_config, true);
    acc_detector_presence_config_intra_detection_set(ctx.presence_config, true);
    acc_detector_presence_config_intra_detection_threshold_set(ctx.presence_config, 1.3f);
    acc_detector_presence_config_inter_detection_set(ctx.presence_config, true);
    acc_detector_presence_config_inter_detection_threshold_set(ctx.presence_config, 1.0f);

    acc_detector_presence_metadata_t metadata;
    ctx.presence_handle = acc_detector_presence_create(ctx.presence_config, &metadata);
    if (!ctx.presence_handle) return false;
    ctx.presence_num_points = metadata.num_points;

    if (!acc_detector_presence_get_buffer_size(ctx.presence_handle, &ctx.buffer_size)) return false;
    
    ctx.buffer = acc_integration_mem_alloc(ctx.buffer_size);
    if (!ctx.buffer) return false;

    acc_hal_integration_sensor_supply_on(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    ctx.sensor = acc_sensor_create(SENSOR_ID);
    if (!ctx.sensor) return false;

    acc_cal_result_t cal_result;
    bool cal_complete = false;
    bool status = false;
    do {
        status = acc_sensor_calibrate(ctx.sensor, &cal_complete, &cal_result, ctx.buffer, ctx.buffer_size);
        if (status && !cal_complete) acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
    } while (status && !cal_complete);

    if (!status) return false;

    if (!acc_detector_presence_prepare(ctx.presence_handle, ctx.presence_config, ctx.sensor, &cal_result, ctx.buffer, ctx.buffer_size)) return false;

    fall_detector_init();
    vital_signs_init();
    ctx.first_phase          = true;
    ctx.unwrapped_angle      = 0.0f;
    ctx.has_previous_presence = false;
    ctx.missing_target_count = 0;
    ctx.last_known_dist      = 0.0f;
    ctx.ema_dist             = 0.0f;

    ctx.det_phase            = DET_SEARCHING;
    ctx.tracked_index        = -1;
    ctx.phase_renorm_counter = 0;
    ctx.n_candidates         = 0;

    return true;
}

/***********************************************************************************************************************
 * @brief   Initialize the radar in the specified mode.
 *          
 *          This initialization sequence performs the following steps:
 *          1. Clear any existing mode.
 *          2. Start specific mode (Vibration, Vital, or Fall).
 * 
 * @param   mode            The mode to start (Vibration, Vital, or Fall).
 * @return  true            if the initialization was successful, false otherwise.
***********************************************************************************************************************/
bool Radar_Adapter_Start(Radar_Mode_t mode) {
    // Stop any existing mode before starting a new one
    Radar_Adapter_Stop();

    // Start specific mode
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

static float update_phase(const acc_detector_presence_result_t *res,
                           uint16_t num_points, uint16_t spf, int bin,
                           float *prev_a, bool *first, float *unwrapped)
{
    float complex mean = 0.0f + 0.0f * I;
    for (int s = 0; s < spf; s++) {
        acc_int16_complex_t p = res->processing_result.frame[s * num_points + bin];
        mean += (float)p.real + (float)p.imag * I;
    }
    mean /= spf;

    float angle = cargf(mean);
    if (*first) {
        *prev_a    = angle;
        *unwrapped = angle;
        *first     = false;
    } else {
        float diff = angle - *prev_a;
        if (diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
        if (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
        *unwrapped += diff;
        *prev_a     = angle;
    }
    return *unwrapped;
}

/***********************************************************************************************************************
 * @brief   Process the radar data in the specified mode.
 * 
 *          This processing sequence performs the following steps:
 *          1. Measures the radar data.
 *          2. Reads the radar data.
 *          3. Processes the radar data.
 *          4. Sends the radar data.
 *          
 * 
 * @param   mode            The mode to process (Vibration, Vital, or Fall).
 * @return  true            if the processing was successful, false otherwise.
***********************************************************************************************************************/
bool Radar_Adapter_Process(Radar_Mode_t mode) {
    if (!ctx.sensor) return false;

    if (!acc_sensor_measure(ctx.sensor)) return false;
    
    if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS)) return false;

    if (!acc_sensor_read(ctx.sensor, ctx.buffer, ctx.buffer_size)) return false;

    if (mode == RADAR_MODE_VIBRATION) {
        acc_processing_result_t proc_result = {0};
        acc_processing_execute(ctx.processing, ctx.buffer, &proc_result);

        if (proc_result.calibration_needed) {
            LOG_INFO_APP("Radar needs re-calibration.\r\n");
            const acc_config_t *cfg = acc_vibration_handle_sensor_config_get(ctx.vib_handle);
            if (cfg) do_sensor_calibration_and_prepare(ctx.sensor, ctx.buffer, ctx.buffer_size, cfg);
            return false; // Skip this frame
        }

        if (ctx.vib_handle) {
            acc_vibration_result_t result = {0};
            acc_vibration_process(&proc_result, ctx.vib_handle, &ctx.vib_config, &result);

            /* Throttled raw diagnostic: print once every 20 frames (~1 s) */
            static uint32_t vib_dbg_counter = 0;
            vib_dbg_counter++;
            if (vib_dbg_counter >= 20) {
                vib_dbg_counter = 0;
                if (result.peak_count == 0) {
                    LOG_INFO_APP("[VIB RAW] No peaks (peak_count=0)\r\n");
                } else {
                    LOG_INFO_APP("[VIB RAW] peaks=%u  #0: freq=%.2f Hz  disp=%.2f um  stability=%lu/%d  disp_ok=%s\r\n",
                        (unsigned)result.peak_count,
                        result.peak_frequencies[0],
                        result.peak_displacements[0],
                        (unsigned long)ctx.vib_stability_counter,
                        STABILITY_REQUIRED,
                        (result.peak_displacements[0] > 5.0f) ? "YES" : "NO (<5um)");
                }
            }

            float top_freq = 0;
            float top_disp = 0;

            if (result.peak_count > 0) {
                float current_freq = result.peak_frequencies[0];
                float current_disp = result.peak_displacements[0];

                /* Check stability: Is this frequency close to the last one? */
                if (fabsf(current_freq - ctx.vib_prev_freq) < STABILITY_THRESHOLD) {
                    ctx.vib_stability_counter++;
                } else {
                    ctx.vib_stability_counter = 0;
                }
                ctx.vib_prev_freq = current_freq;

                /* Only report if stable for N frames and above displacement threshold */
                if (ctx.vib_stability_counter >= STABILITY_REQUIRED && current_disp > 5.0f) {
                    top_freq = current_freq;
                    top_disp = current_disp;

                    float omega        = 2.0f * (float)M_PI * top_freq;
                    float velocity     = (top_disp * omega) / 1e3f;
                    float acceleration = (top_disp * omega * omega) / 1e6f;
                    float disp_rms     = top_disp  / (float)M_SQRT2;
                    float vel_rms      = velocity   / (float)M_SQRT2;
                    float accel_rms    = acceleration / (float)M_SQRT2;

                    LOG_INFO_APP("[FILTERED VIB] Freq=%.2f Hz Disp=%.2f um Vel=%.2f mm/s Accel=%.2f m/s^2\r\n",
                                 top_freq, top_disp, velocity, acceleration);

                    VIBRATION_APP_UpdateData(top_freq, top_disp, disp_rms, velocity, vel_rms, acceleration, accel_rms);
                    ctx.vib_was_stable = true;
                } else if (ctx.vib_was_stable) {
                    /* Vibration just stopped or became unstable - send one '0' update to clear the app */
                    VIBRATION_APP_UpdateData(0, 0, 0, 0, 0, 0, 0);
                    ctx.vib_was_stable = false;
                }
            } else {
                ctx.vib_stability_counter = 0;
                if (ctx.vib_was_stable) {
                    VIBRATION_APP_UpdateData(0, 0, 0, 0, 0, 0, 0);
                    ctx.vib_was_stable = false;
                }
            }
        }
    } else if (mode == RADAR_MODE_VITAL || mode == RADAR_MODE_FALL) {
        if (!ctx.presence_handle) return false;

        acc_detector_presence_result_t result;
        if (!acc_detector_presence_process(ctx.presence_handle, ctx.buffer, &result)) return false;

        if (result.processing_result.calibration_needed) {
            LOG_INFO_APP("Sensor recalibration needed ... \r\n");
            acc_cal_result_t cal_result;
            bool cal_complete = false;
            bool status = false;
            do {
                status = acc_sensor_calibrate(ctx.sensor, &cal_complete, &cal_result, ctx.buffer, ctx.buffer_size);
                if (status && !cal_complete) acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
            } while (status && !cal_complete);
            if (status) acc_detector_presence_prepare(ctx.presence_handle, ctx.presence_config, ctx.sensor, &cal_result, ctx.buffer, ctx.buffer_size);
            return false;
        }

        if (result.depthwise_presence_scores_length < 2) return true;

        const float RANGE_START = 0.3f;
        const float RANGE_END   = 2.5f;
        float step_length = (RANGE_END - RANGE_START) / (float)(result.depthwise_presence_scores_length - 1);

        int prev_tracked = ctx.tracked_index;

        /* --- Smart Target Lock --- */
        if (result.presence_detected) {
            ctx.missing_target_count = 0;
            if (ctx.tracked_index == -1) {
                float max_s = 0.0f;
                for (uint32_t i = 0; i < result.depthwise_presence_scores_length; i++) {
                    if (result.depthwise_inter_presence_scores[i] > max_s) {
                        max_s = result.depthwise_inter_presence_scores[i];
                        ctx.tracked_index = (int)i;
                    }
                }
            } else {
                float locked_dist = RANGE_START + ctx.tracked_index * step_length;
                if (fabsf(result.presence_distance - locked_dist) > 0.4f) {
                    ctx.tracked_index = -1;
                    ctx.ema_dist      = 0.0f;
                }
            }
        } else {
            if (ctx.tracked_index != -1) {
                ctx.missing_target_count++;
                if (ctx.missing_target_count > (uint32_t)(10.0f * RADAR_FRAME_RATE)) {
                    ctx.tracked_index        = -1;
                    ctx.ema_dist             = 0.0f;
                    ctx.missing_target_count = 0;
                }
            } else {
                ctx.ema_dist = 0.0f;
            }
        }

        uint16_t spf = acc_detector_presence_config_sweeps_per_frame_get(ctx.presence_config);

        /* --- Transition: new target → start coarse scan --- */
        if (prev_tracked == -1 && ctx.tracked_index != -1) {
            ctx.det_phase    = DET_COARSE;
            ctx.n_candidates = 0;
            for (int b = ctx.tracked_index - 1; b <= ctx.tracked_index + 1; b++) {
                if (b >= 0 && b < (int)result.depthwise_presence_scores_length
                           && ctx.n_candidates < COARSE_MAX_CANDS) {
                    ctx.candidate_bins[ctx.n_candidates]   = b;
                    ctx.cand_prev_angle[ctx.n_candidates]  = 0.0f;
                    ctx.cand_unwrapped[ctx.n_candidates]   = 0.0f;
                    ctx.cand_first_phase[ctx.n_candidates] = true;
                    ctx.n_candidates++;
                }
            }
            vital_signs_coarse_start();
            LOG_INFO_APP("[System] Target appeared, starting coarse sweep (%d candidates)\r\n", ctx.n_candidates);
        }

        /* --- Transition: target lost → back to searching --- */
        if (prev_tracked != -1 && ctx.tracked_index == -1) {
            vital_signs_init();
            ctx.first_phase          = true;
            ctx.phase_renorm_counter = 0;
            ctx.det_phase            = DET_SEARCHING;
        }

        /* --- Main state machine --- */
        if (ctx.tracked_index != -1) {
            float raw_dist = RANGE_START + ctx.tracked_index * step_length;
            if (ctx.ema_dist == 0.0f) ctx.ema_dist = raw_dist;
            else ctx.ema_dist = 0.15f * raw_dist + 0.85f * ctx.ema_dist;
            float current_dist = ctx.ema_dist;

            if (ctx.det_phase == DET_COARSE) {
                for (int c = 0; c < ctx.n_candidates; c++) {
                    float ua = update_phase(&result, ctx.presence_num_points, spf,
                                            ctx.candidate_bins[c],
                                            &ctx.cand_prev_angle[c],
                                            &ctx.cand_first_phase[c],
                                            &ctx.cand_unwrapped[c]);
                    vital_signs_coarse_feed(c, ua);
                }
                if (vital_signs_coarse_tick()) {
                    int best = vital_signs_coarse_pick_best(ctx.n_candidates);
                    if (best >= 0) {
                        ctx.tracked_index        = ctx.candidate_bins[best];
                        ctx.prev_angle           = ctx.cand_prev_angle[best];
                        ctx.unwrapped_angle      = ctx.cand_unwrapped[best];
                        ctx.first_phase          = false;
                        ctx.phase_renorm_counter = 0;
                        ctx.det_phase            = DET_MEASURING;
                        vital_signs_init();
                        vital_signs_replay_coarse(best);
                        LOG_INFO_APP("[System] Coarse done! Best dist %.2fm, switching to fine mode\r\n",
                                     RANGE_START + ctx.tracked_index * step_length);
                    } else {
                        LOG_INFO_APP("[System] Coarse failed, searching again\r\n");
                        ctx.tracked_index = -1;
                        ctx.ema_dist      = 0.0f;
                        ctx.det_phase     = DET_SEARCHING;
                    }
                }

            } else if (ctx.det_phase == DET_MEASURING) {
                if (++ctx.phase_renorm_counter >= (uint32_t)(600.0f * RADAR_FRAME_RATE)) {
                    ctx.first_phase          = true;
                    ctx.phase_renorm_counter = 0;
                    vital_signs_init();
                }

                update_phase(&result, ctx.presence_num_points, spf, ctx.tracked_index,
                             &ctx.prev_angle, &ctx.first_phase, &ctx.unwrapped_angle);

                process_vital_signs(ctx.unwrapped_angle, current_dist);
                process_fall_detection(result.intra_presence_score, current_dist);
            }
        }
    }
    
    return true;
}

/***********************************************************************************************************************
 * @brief   Stop the radar and clean up memory.
 * 
 *          This cleanup sequence performs the following steps:
 *          1. Disables the sensor.
 *          2. Turns off the sensor supply.
 *          3. Destroys the sensor.
 *          4. Destroys the processing instance.
 *          5. Frees the working buffer.
 *          6. Clears the vibration stability counter and previous frequency.
 *          7. Destroys the mode-specific handle.
 *          
 * 
 * @return  void
***********************************************************************************************************************/
void Radar_Adapter_Stop(void) {
    LOG_INFO_APP("Radar Adapter: Stopping and cleaning memory...\r\n");
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_supply_off(SENSOR_ID);

    if (ctx.sensor) { acc_sensor_destroy(ctx.sensor); ctx.sensor = NULL; }
    if (ctx.processing) { acc_processing_destroy(ctx.processing); ctx.processing = NULL; }
    if (ctx.buffer) { acc_integration_mem_free(ctx.buffer); ctx.buffer = NULL; }
    
    ctx.vib_stability_counter = 0;
    ctx.vib_prev_freq = 0.0f;
    ctx.vib_was_stable = false;
    
    /* --- Mode-Specific Handle Destruction --- */
    if (ctx.vib_handle) { acc_vibration_handle_destroy(ctx.vib_handle); ctx.vib_handle = NULL; }
    
    if (ctx.presence_handle) { acc_detector_presence_destroy(ctx.presence_handle); ctx.presence_handle = NULL; }
    if (ctx.presence_config) { acc_detector_presence_config_destroy(ctx.presence_config); ctx.presence_config = NULL; }
}

/***********************************************************************************************************************
 * @brief   Helper function to perform sensor calibration and preparation.
 * 
 *          This helper function performs the following steps:
 *          1. Disables the sensor.
 *          2. Enables the sensor.
 *          3. Performs sensor calibration.
 *          4. Waits for sensor interrupt.
 *          5. Prepares the sensor for the specified mode.
 *          
 * 
 * @param   s               Pointer to the sensor.
 * @param   b               Pointer to the working buffer.
 * @param   b_size          Size of the working buffer.
 * @param   s_config        Pointer to the sensor configuration.
 * @return  true            if the calibration and preparation were successful, false otherwise.
***********************************************************************************************************************/
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
