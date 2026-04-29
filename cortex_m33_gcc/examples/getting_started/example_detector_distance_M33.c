// Copyright (c) Acconeer AB, 2022-2025
// All rights reserved
// This file is subject to the terms and conditions defined in the file
// 'LICENSES/license_acconeer.txt', (BSD 3-Clause License) which is part
// of this source code package.

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc_definitions_a121.h"
#include "acc_detector_distance.h"
#include "acc_hal_definitions_a121.h"
#include "acc_hal_integration_a121.h"
#include "acc_integration.h"
#include "acc_integration_log.h"
#include "acc_rss_a121.h"
#include "acc_sensor.h"
#include "acc_version.h"

#define PI 3.14159265358979323846f

typedef struct {
  float real;
  float imag;
} complex_t;

static void bit_reverse(complex_t *x, int n) {
  int i, j, k;
  for (i = 1, j = 0; i < n - 1; i++) {
    for (k = n >> 1; (!((j ^= k) & k)); k >>= 1)
      ;
    if (i < j) {
      complex_t temp = x[i];
      x[i] = x[j];
      x[j] = temp;
    }
  }
}

static void compute_fft(complex_t *x, int n) {
  bit_reverse(x, n);
  for (int step = 1; step < n; step <<= 1) {
    float theta = -PI / step;
    float w_real_step = cosf(theta);
    float w_imag_step = sinf(theta);
    for (int i = 0; i < n; i += 2 * step) {
      float w_real = 1.0f;
      float w_imag = 0.0f;
      for (int j = 0; j < step; j++) {
        int a = i + j;
        int b = i + j + step;

        float t_real = w_real * x[b].real - w_imag * x[b].imag;
        float t_imag = w_real * x[b].imag + w_imag * x[b].real;

        x[b].real = x[a].real - t_real;
        x[b].imag = x[a].imag - t_imag;
        x[a].real += t_real;
        x[a].imag += t_imag;

        float w_real_next = w_real * w_real_step - w_imag * w_imag_step;
        float w_imag_next = w_real * w_imag_step + w_imag * w_real_step;
        w_real = w_real_next;
        w_imag = w_imag_next;
      }
    }
  }
}

typedef enum {
  DISTANCE_PRESET_CONFIG_NONE = 0,
  DISTANCE_PRESET_CONFIG_BALANCED,
  DISTANCE_PRESET_CONFIG_HIGH_ACCURACY,
} distance_preset_config_t;

#define SENSOR_ID (1U)
// 2 seconds should be enough even for long ranges and high signal quality
#define SENSOR_TIMEOUT_MS (2000U)

typedef struct {
  acc_sensor_t *sensor;
  acc_detector_distance_config_t *config;
  acc_detector_distance_handle_t *handle;
  void *buffer;
  uint32_t buffer_size;
  uint8_t *detector_cal_result_static;
  uint32_t detector_cal_result_static_size;
  acc_detector_cal_result_dynamic_t detector_cal_result_dynamic;
} distance_detector_resources_t;

static void cleanup(distance_detector_resources_t *resources);

static void set_config(acc_detector_distance_config_t *detector_config,
                       distance_preset_config_t preset);

static bool
initialize_detector_resources(distance_detector_resources_t *resources);

static bool do_sensor_calibration(acc_sensor_t *sensor,
                                  acc_cal_result_t *sensor_cal_result,
                                  void *buffer, uint32_t buffer_size);

static bool
do_full_detector_calibration(distance_detector_resources_t *resources,
                             const acc_cal_result_t *sensor_cal_result);

static bool
do_detector_calibration_update(distance_detector_resources_t *resources,
                               const acc_cal_result_t *sensor_cal_result);

static bool do_detector_get_next(distance_detector_resources_t *resources,
                                 const acc_cal_result_t *sensor_cal_result,
                                 acc_detector_distance_result_t *result);

// --- MODIFICATION: Tracking variables ---
static float previous_distance = 0.0f;
static bool has_previous_distance = false;

#define FFT_N 128
#define SAMPLE_RATE_HZ 20.0f
static float distance_history[FFT_N] = {0};
static uint16_t dist_idx = 0;
static bool buffer_full = false;
static uint16_t slide_counter = 0;
// ----------------------------------------

static void print_distance_result(const acc_detector_distance_result_t *result);

int acc_example_detector_distance(int argc, char *argv[]);

int acc_example_detector_distance(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  distance_detector_resources_t resources = {0};

  printf("Acconeer software version %s\n", acc_version_get());

  const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();

  if (!acc_rss_hal_register(hal)) {
    return EXIT_FAILURE;
  }

  resources.config = acc_detector_distance_config_create();
  if (resources.config == NULL) {
    printf("acc_detector_distance_config_create() failed\n");
    cleanup(&resources);
    return EXIT_FAILURE;
  }

  set_config(resources.config, DISTANCE_PRESET_CONFIG_BALANCED);

  if (!initialize_detector_resources(&resources)) {
    printf("Initializing detector resources failed\n");
    cleanup(&resources);
    return EXIT_FAILURE;
  }

  // Print the configuration
  acc_detector_distance_config_log(resources.handle, resources.config);

  /* Turn the sensor on */
  acc_hal_integration_sensor_supply_on(SENSOR_ID);
  acc_hal_integration_sensor_enable(SENSOR_ID);

  resources.sensor = acc_sensor_create(SENSOR_ID);
  if (resources.sensor == NULL) {
    printf("acc_sensor_create() failed\n");
    cleanup(&resources);
    return EXIT_FAILURE;
  }

  acc_cal_result_t sensor_cal_result;

  if (!do_sensor_calibration(resources.sensor, &sensor_cal_result,
                             resources.buffer, resources.buffer_size)) {
    printf("Sensor calibration failed\n");
    cleanup(&resources);
    return EXIT_FAILURE;
  }

  if (!do_full_detector_calibration(&resources, &sensor_cal_result)) {
    printf("Detector calibration failed\n");
    cleanup(&resources);
    return EXIT_FAILURE;
  }

  // Reset tracking variables
  previous_distance = 0.0f;
  has_previous_distance = false;
  dist_idx = 0;
  buffer_full = false;
  slide_counter = 0;

  acc_integration_set_periodic_wakeup(50); // 50 ms = 20 Hz update rate

  while (true) {
    acc_detector_distance_result_t result = {0};

    if (!do_detector_get_next(&resources, &sensor_cal_result, &result)) {
      printf("Could not get next result\n");
      cleanup(&resources);
      return EXIT_FAILURE;
    }

    /* If "calibration needed" is indicated, the sensor needs to be recalibrated
     * and the detector calibration updated */
    if (result.calibration_needed) {
      printf(
          "Sensor recalibration and detector calibration update needed ... \n");

      if (!do_sensor_calibration(resources.sensor, &sensor_cal_result,
                                 resources.buffer, resources.buffer_size)) {
        printf("Sensor calibration failed\n");
        cleanup(&resources);
        return EXIT_FAILURE;
      }

      /* Once the sensor is recalibrated, the detector calibration should be
       * updated and measuring can continue. */
      if (!do_detector_calibration_update(&resources, &sensor_cal_result)) {
        printf("Detector calibration update failed\n");
        cleanup(&resources);
        return EXIT_FAILURE;
      }

      printf("Sensor recalibration and detector calibration update done!\n");
    } else {
      print_distance_result(&result);
    }

    acc_integration_sleep_until_periodic_wakeup();
  }

  acc_integration_set_periodic_wakeup(0);
  cleanup(&resources);

  printf("Done!\n");

  return EXIT_SUCCESS;
}

static void cleanup(distance_detector_resources_t *resources) {
  acc_hal_integration_sensor_disable(SENSOR_ID);
  acc_hal_integration_sensor_supply_off(SENSOR_ID);

  acc_detector_distance_config_destroy(resources->config);
  acc_detector_distance_destroy(resources->handle);

  acc_integration_mem_free(resources->buffer);
  acc_integration_mem_free(resources->detector_cal_result_static);

  if (resources->sensor != NULL) {
    acc_sensor_destroy(resources->sensor);
  }
}

static void set_config(acc_detector_distance_config_t *detector_config,
                       distance_preset_config_t preset) {
  switch (preset) {
  case DISTANCE_PRESET_CONFIG_NONE:
    break;

  case DISTANCE_PRESET_CONFIG_BALANCED:
    acc_detector_distance_config_start_set(detector_config, 0.2f);
    acc_detector_distance_config_end_set(detector_config, 1.0f);
    acc_detector_distance_config_max_step_length_set(detector_config, 0U);
    acc_detector_distance_config_max_profile_set(detector_config,
                                                 ACC_CONFIG_PROFILE_5);
    acc_detector_distance_config_reflector_shape_set(
        detector_config, ACC_DETECTOR_DISTANCE_REFLECTOR_SHAPE_GENERIC);
    acc_detector_distance_config_peak_sorting_set(
        detector_config, ACC_DETECTOR_DISTANCE_PEAK_SORTING_STRONGEST);
    acc_detector_distance_config_threshold_method_set(
        detector_config, ACC_DETECTOR_DISTANCE_THRESHOLD_METHOD_CFAR);
    acc_detector_distance_config_threshold_sensitivity_set(detector_config,
                                                           0.5f);
    acc_detector_distance_config_signal_quality_set(detector_config, 15.0f);
    acc_detector_distance_config_close_range_leakage_cancellation_set(
        detector_config, false);
    break;

  case DISTANCE_PRESET_CONFIG_HIGH_ACCURACY:
    acc_detector_distance_config_start_set(detector_config, 0.25f);
    acc_detector_distance_config_end_set(detector_config, 3.0f);
    acc_detector_distance_config_max_step_length_set(detector_config, 2U);
    acc_detector_distance_config_max_profile_set(detector_config,
                                                 ACC_CONFIG_PROFILE_3);
    acc_detector_distance_config_reflector_shape_set(
        detector_config, ACC_DETECTOR_DISTANCE_REFLECTOR_SHAPE_GENERIC);
    acc_detector_distance_config_peak_sorting_set(
        detector_config, ACC_DETECTOR_DISTANCE_PEAK_SORTING_STRONGEST);
    acc_detector_distance_config_threshold_method_set(
        detector_config, ACC_DETECTOR_DISTANCE_THRESHOLD_METHOD_CFAR);
    acc_detector_distance_config_threshold_sensitivity_set(detector_config,
                                                           0.5f);
    acc_detector_distance_config_signal_quality_set(detector_config, 20.0f);
    acc_detector_distance_config_close_range_leakage_cancellation_set(
        detector_config, false);
    break;
  }
}

static bool
initialize_detector_resources(distance_detector_resources_t *resources) {
  resources->handle = acc_detector_distance_create(resources->config);
  if (resources->handle == NULL) {
    printf("acc_detector_distance_create() failed\n");
    return false;
  }

  if (!acc_detector_distance_get_sizes(
          resources->handle, &(resources->buffer_size),
          &(resources->detector_cal_result_static_size))) {
    printf("acc_detector_distance_get_sizes() failed\n");
    return false;
  }

  resources->buffer = acc_integration_mem_alloc(resources->buffer_size);
  if (resources->buffer == NULL) {
    printf("sensor buffer allocation failed\n");
    return false;
  }

  resources->detector_cal_result_static =
      acc_integration_mem_alloc(resources->detector_cal_result_static_size);
  if (resources->detector_cal_result_static == NULL) {
    printf("calibration buffer allocation failed\n");
    return false;
  }

  return true;
}

static bool do_sensor_calibration(acc_sensor_t *sensor,
                                  acc_cal_result_t *sensor_cal_result,
                                  void *buffer, uint32_t buffer_size) {
  bool status = false;
  bool cal_complete = false;
  const uint16_t calibration_retries = 1U;

  for (uint16_t i = 0; !status && (i <= calibration_retries); i++) {
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    do {
      status = acc_sensor_calibrate(sensor, &cal_complete, sensor_cal_result,
                                    buffer, buffer_size);

      if (status && !cal_complete) {
        status = acc_hal_integration_wait_for_sensor_interrupt(
            SENSOR_ID, SENSOR_TIMEOUT_MS);
      }
    } while (status && !cal_complete);
  }

  if (status) {
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);
  }

  return status;
}

static bool
do_full_detector_calibration(distance_detector_resources_t *resources,
                             const acc_cal_result_t *sensor_cal_result) {
  bool done = false;
  bool status;

  do {
    status = acc_detector_distance_calibrate(
        resources->sensor, resources->handle, sensor_cal_result,
        resources->buffer, resources->buffer_size,
        resources->detector_cal_result_static,
        resources->detector_cal_result_static_size,
        &resources->detector_cal_result_dynamic, &done);

    if (status && !done) {
      status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID,
                                                             SENSOR_TIMEOUT_MS);
    }
  } while (status && !done);

  return status;
}

static bool
do_detector_calibration_update(distance_detector_resources_t *resources,
                               const acc_cal_result_t *sensor_cal_result) {
  bool done = false;
  bool status;

  do {
    status = acc_detector_distance_update_calibration(
        resources->sensor, resources->handle, sensor_cal_result,
        resources->buffer, resources->buffer_size,
        &resources->detector_cal_result_dynamic, &done);

    if (status && !done) {
      status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID,
                                                             SENSOR_TIMEOUT_MS);
    }
  } while (status && !done);

  return status;
}

static bool do_detector_get_next(distance_detector_resources_t *resources,
                                 const acc_cal_result_t *sensor_cal_result,
                                 acc_detector_distance_result_t *result) {
  bool result_available = false;

  do {
    if (!acc_detector_distance_prepare(
            resources->handle, resources->config, resources->sensor,
            sensor_cal_result, resources->buffer, resources->buffer_size)) {
      printf("acc_detector_distance_prepare() failed\n");
      return false;
    }

    if (!acc_sensor_measure(resources->sensor)) {
      printf("acc_sensor_measure() failed\n");
      return false;
    }

    if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID,
                                                       SENSOR_TIMEOUT_MS)) {
      printf("Sensor interrupt timeout\n");
      return false;
    }

    if (!acc_sensor_read(resources->sensor, resources->buffer,
                         resources->buffer_size)) {
      printf("acc_sensor_read() failed\n");
      return false;
    }

    if (!acc_detector_distance_process(resources->handle, resources->buffer,
                                       resources->detector_cal_result_static,
                                       &resources->detector_cal_result_dynamic,
                                       &result_available, result)) {
      printf("acc_detector_distance_process() failed\n");
      return false;
    }
  } while (!result_available);

  return true;
}

static void
print_distance_result(const acc_detector_distance_result_t *result) {
  if (result->num_distances == 0) {
    slide_counter++;
    if (slide_counter >= 20) {
      printf("0 detected distances. Keep sensor aimed at target.\n");
      slide_counter = 0;
    }
    return;
  }

  float current_distance = result->distances[0];

  float difference = 0.0f;
  if (has_previous_distance) {
    difference = current_distance - previous_distance;
  } else {
    has_previous_distance = true;
  }

  previous_distance = current_distance;

  distance_history[dist_idx] = difference;
  dist_idx++;
  if (dist_idx >= FFT_N) {
    dist_idx = 0;
    buffer_full = true;
  }

  slide_counter++;

  if (!buffer_full && slide_counter >= 10) {
    printf("Detected distance: %" PRIfloat " m | Buffering data... %d%%\n",
           ACC_LOG_FLOAT_TO_INTEGER(current_distance),
           (dist_idx * 100) / FFT_N);
    slide_counter = 0;
  } else if (buffer_full && slide_counter >= 20) {
    slide_counter = 0;
    complex_t fft_data[FFT_N];
    float mean = 0.0f;

    for (int k = 0; k < FFT_N; k++) {
      float val = distance_history[(dist_idx + k) % FFT_N];
      fft_data[k].real = val;
      fft_data[k].imag = 0.0f;
      mean += val;
    }
    mean /= FFT_N;

    for (int k = 0; k < FFT_N; k++) {
      fft_data[k].real -= mean;
    }

    compute_fft(fft_data, FFT_N);

    int min_bin = 5;  // ~0.8 Hz
    int max_bin = 19; // ~3.0 Hz
    float max_mag = -1.0f;
    int peak_bin = min_bin;

    for (int k = min_bin; k <= max_bin; k++) {
      float mag = sqrtf(fft_data[k].real * fft_data[k].real +
                        fft_data[k].imag * fft_data[k].imag);
      if (mag > max_mag) {
        max_mag = mag;
        peak_bin = k;
      }
    }

    float peak_freq = (float)peak_bin * SAMPLE_RATE_HZ / FFT_N;
    float peak_bpm = peak_freq * 60.0f;
    printf("=======================================================\n");
    printf(" Target distance: %" PRIfloat " m\n",
           ACC_LOG_FLOAT_TO_INTEGER(current_distance));
    printf(" Heartbeat Frequency: %" PRIfloat " Hz (%" PRIfloat " BPM)\n",
           ACC_LOG_FLOAT_TO_INTEGER(peak_freq),
           ACC_LOG_FLOAT_TO_INTEGER(peak_bpm));
    printf("=======================================================\n");
  }
}
