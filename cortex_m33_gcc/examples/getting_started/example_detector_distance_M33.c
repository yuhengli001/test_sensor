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
#include <complex.h>

#include "acc_definitions_a121.h"
#include "acc_detector_presence.h"
#include "acc_hal_definitions_a121.h"
#include "acc_hal_integration_a121.h"
#include "acc_integration.h"
#include "acc_integration_log.h"
#include "acc_rss_a121.h"
#include "acc_sensor.h"
#include "acc_version.h"

#include "app_config.h"
#include "fall_detector.h"
#include "vital_signs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define SENSOR_ID (1U)
#define SENSOR_TIMEOUT_MS (2000U)

static bool do_sensor_calibration(acc_sensor_t *sensor, acc_cal_result_t *cal_result, void *buffer, uint32_t buffer_size);

static void cleanup(acc_detector_presence_handle_t *presence_handle,
                    acc_detector_presence_config_t *presence_config,
                    acc_sensor_t                   *sensor,
                    void                           *buffer);

// 相位追踪相关的静态变量
static float prev_angle = 0.0f;
static float unwrapped_angle = 0.0f;
static bool first_phase = true;
static float previous_presence_dist = 0.0f;
static bool has_previous_presence = false;
static uint32_t missing_target_count = 0;

static float last_known_dist = 0.0f;
static float ema_dist = 0.0f;

int acc_example_detector_distance(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  acc_detector_presence_config_t  *presence_config = NULL;
  acc_detector_presence_handle_t  *presence_handle = NULL;
  acc_detector_presence_metadata_t metadata;
  acc_sensor_t                    *sensor      = NULL;
  void                            *buffer      = NULL;
  uint32_t                         buffer_size = 0U;

  printf("Acconeer software version %s\n", acc_version_get());

  const acc_hal_a121_t *hal = acc_hal_rss_integration_get_implementation();

  if (!acc_rss_hal_register(hal)) {
    return EXIT_FAILURE;
  }

  presence_config = acc_detector_presence_config_create();
  if (presence_config == NULL) {
    printf("acc_detector_presence_config_create() failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  // 配置存在感探测器 (专门针对呼吸和微弱移动优化)
  acc_detector_presence_config_start_set(presence_config, 0.3f);
  acc_detector_presence_config_end_set(presence_config, 2.5f);
  acc_detector_presence_config_automatic_subsweeps_set(presence_config, true);
  acc_detector_presence_config_signal_quality_set(presence_config, 20.0f);
  acc_detector_presence_config_sweeps_per_frame_set(presence_config, 16);
  acc_detector_presence_config_frame_rate_set(presence_config, SAMPLE_RATE_HZ);
  acc_detector_presence_config_frame_rate_app_driven_set(presence_config, false);
  acc_detector_presence_config_reset_filters_on_prepare_set(presence_config, true);
  acc_detector_presence_config_intra_detection_set(presence_config, true);
  acc_detector_presence_config_intra_detection_threshold_set(presence_config, 1.3f);
  acc_detector_presence_config_inter_detection_set(presence_config, true);
  acc_detector_presence_config_inter_detection_threshold_set(presence_config, 1.0f);

  presence_handle = acc_detector_presence_create(presence_config, &metadata);
  if (presence_handle == NULL) {
    printf("acc_detector_presence_create() failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  if (!acc_detector_presence_get_buffer_size(presence_handle, &buffer_size)) {
    printf("acc_detector_presence_get_buffer_size() failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  buffer = acc_integration_mem_alloc(buffer_size);
  if (buffer == NULL) {
    printf("buffer allocation failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  acc_hal_integration_sensor_supply_on(SENSOR_ID);
  acc_hal_integration_sensor_enable(SENSOR_ID);

  sensor = acc_sensor_create(SENSOR_ID);
  if (sensor == NULL) {
    printf("acc_sensor_create() failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  acc_cal_result_t cal_result;

  if (!do_sensor_calibration(sensor, &cal_result, buffer, buffer_size)) {
    printf("do_sensor_calibration() failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  if (!acc_detector_presence_prepare(presence_handle, presence_config, sensor, &cal_result, buffer, buffer_size)) {
    printf("acc_detector_presence_prepare() failed\n");
    cleanup(presence_handle, presence_config, sensor, buffer);
    return EXIT_FAILURE;
  }

  // 初始化子模块
  fall_detector_init();
  vital_signs_init();
  first_phase = true;
  unwrapped_angle = 0.0f;
  has_previous_presence = false;
  missing_target_count = 0;

  printf("Entering main loop with Presence Detector (Phase Mode)...\n");

  while (true) {
    acc_detector_presence_result_t result;

    if (!acc_sensor_measure(sensor)) {
      printf("acc_sensor_measure failed\n");
      cleanup(presence_handle, presence_config, sensor, buffer);
      return EXIT_FAILURE;
    }

    if (!acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS)) {
      printf("Sensor interrupt timeout\n");
      cleanup(presence_handle, presence_config, sensor, buffer);
      return EXIT_FAILURE;
    }

    if (!acc_sensor_read(sensor, buffer, buffer_size)) {
      printf("acc_sensor_read failed\n");
      cleanup(presence_handle, presence_config, sensor, buffer);
      return EXIT_FAILURE;
    }

    if (!acc_detector_presence_process(presence_handle, buffer, &result)) {
      printf("acc_detector_presence_process failed\n");
      cleanup(presence_handle, presence_config, sensor, buffer);
      return EXIT_FAILURE;
    }

    // 检查是否需要重新校准
    if (result.processing_result.calibration_needed) {
      printf("Sensor recalibration needed ... \n");
      if (!do_sensor_calibration(sensor, &cal_result, buffer, buffer_size)) {
        printf("do_sensor_calibration() failed\n");
        cleanup(presence_handle, presence_config, sensor, buffer);
        return EXIT_FAILURE;
      }
      if (!acc_detector_presence_prepare(presence_handle, presence_config, sensor, &cal_result, buffer, buffer_size)) {
        printf("acc_detector_presence_prepare() failed\n");
        cleanup(presence_handle, presence_config, sensor, buffer);
        return EXIT_FAILURE;
      }
      continue; // 跳过这一帧的处理
    }

    // 核心处理逻辑：存在感检测 + IQ 相位提取
    if (result.presence_detected) {
      last_known_dist = result.presence_distance;
      missing_target_count = 0;
    } else {
      missing_target_count++;
    }

    // 只要目标没有丢失超过 2 秒，就强制持续提取相位 (解决没输出的问题)
    if (last_known_dist > 0.1f && missing_target_count < (uint32_t)(SAMPLE_RATE_HZ * 2.0f)) {
      float current_dist = last_known_dist;
      
      // 1. 处理跌倒检测逻辑 (引入 EMA 滤波，解决6m/s的误报跳变)
      if (!has_previous_presence) {
        ema_dist = current_dist;
        previous_presence_dist = current_dist;
        has_previous_presence = true;
      } else {
        // 使用 EMA (指数移动平均) 平滑距离跳变，alpha = 0.15
        ema_dist = 0.15f * current_dist + 0.85f * ema_dist;
        
        float dist_diff = ema_dist - previous_presence_dist;
        float velocity = dist_diff * SAMPLE_RATE_HZ;
        process_fall_detection(velocity, dist_diff, ema_dist);
        previous_presence_dist = ema_dist;
      }

      // 2. 提取 IQ 相位用于呼吸心率检测
      // 由于开启了 automatic_subsweeps, step_length_m 可能无效。
      // 我们直接通过寻找 inter_presence_scores 的最高点来确定目标在数组中的 index
      int index = 0;
      float max_score = 0.0f;
      for (uint32_t i = 0; i < result.depthwise_presence_scores_length; i++) {
          if (result.depthwise_inter_presence_scores[i] > max_score) {
              max_score = result.depthwise_inter_presence_scores[i];
              index = (int)i;
          }
      }
      
      if (index >= 0 && index < metadata.num_points) {
        float complex mean_sweep = 0.0f + 0.0f * I;
        
        uint16_t sweeps_per_frame = acc_detector_presence_config_sweeps_per_frame_get(presence_config);
        
        // 计算这一帧所有 sweeps 的平均 IQ 向量 (降低随机噪声)
        for (int s = 0; s < sweeps_per_frame; s++) {
            acc_int16_complex_t point = result.processing_result.frame[s * metadata.num_points + index];
            mean_sweep += (float)point.real + (float)point.imag * I;
        }
        mean_sweep /= sweeps_per_frame;

        // 计算夹角 (相位)
        float angle = cargf(mean_sweep);

        // 相位解包裹 (Unwrapping)
        if (first_phase) {
            prev_angle = angle;
            unwrapped_angle = angle;
            first_phase = false;
        } else {
            float angle_diff = angle - prev_angle;
            
            // 处理跨越 2*PI 的跳变
            if (angle_diff > M_PI) {
                angle_diff -= 2.0f * M_PI;
            } else if (angle_diff < -M_PI) {
                angle_diff += 2.0f * M_PI;
            }
            
            unwrapped_angle += angle_diff;
            prev_angle = angle;
        }

        // 将解包裹后的相位传入维生体征模块
        process_vital_signs(unwrapped_angle, current_dist);
      }
    } else if (missing_target_count >= (uint32_t)(SAMPLE_RATE_HZ * 2.0f)) {
      // 真正目标丢失 (超过2秒)
      if (has_previous_presence) {
          printf("[System] Target lost.\n");
      } else if (missing_target_count % (uint32_t)(SAMPLE_RATE_HZ * 2.0f) == 0) {
          // 每两秒打印一次扫描状态，防止完全没输出让用户误以为死机
          printf("[System] Scanning for target... (Scores: intra=%.2f, inter=%.2f)\n", 
                 result.intra_presence_score, result.inter_presence_score);
      }
      has_previous_presence = false;
      first_phase = true;
      last_known_dist = 0.0f;
    }
  }

  cleanup(presence_handle, presence_config, sensor, buffer);
  return EXIT_SUCCESS;
}

static void cleanup(acc_detector_presence_handle_t *presence_handle,
                    acc_detector_presence_config_t *presence_config,
                    acc_sensor_t                   *sensor,
                    void                           *buffer)
{
  acc_hal_integration_sensor_disable(SENSOR_ID);
  acc_hal_integration_sensor_supply_off(SENSOR_ID);

  if (presence_config != NULL) {
    acc_detector_presence_config_destroy(presence_config);
  }

  if (presence_handle != NULL) {
    acc_detector_presence_destroy(presence_handle);
  }

  if (sensor != NULL) {
    acc_sensor_destroy(sensor);
  }

  if (buffer != NULL) {
    acc_integration_mem_free(buffer);
  }
}

static bool do_sensor_calibration(acc_sensor_t *sensor, acc_cal_result_t *cal_result, void *buffer, uint32_t buffer_size)
{
  bool           status              = false;
  bool           cal_complete        = false;
  const uint16_t calibration_retries = 1U;

  for (uint16_t i = 0; !status && (i <= calibration_retries); i++) {
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);

    do {
      status = acc_sensor_calibrate(sensor, &cal_complete, cal_result, buffer, buffer_size);
      if (status && !cal_complete) {
        status = acc_hal_integration_wait_for_sensor_interrupt(SENSOR_ID, SENSOR_TIMEOUT_MS);
      }
    } while (status && !cal_complete);
  }

  if (status) {
    acc_hal_integration_sensor_disable(SENSOR_ID);
    acc_hal_integration_sensor_enable(SENSOR_ID);
  }

  return status;
}
