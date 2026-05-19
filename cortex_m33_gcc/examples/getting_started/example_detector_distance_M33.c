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

// 从指定 bin 提取相位并做角度解包，返回当前 unwrapped 值
static float update_phase(const acc_detector_presence_result_t  *res,
                           const acc_detector_presence_metadata_t *meta,
                           uint16_t spf, int bin,
                           float *prev_a, bool *first, float *unwrapped)
{
    float complex mean = 0.0f + 0.0f * I;
    for (int s = 0; s < spf; s++) {
        acc_int16_complex_t p = res->processing_result.frame[s * meta->num_points + bin];
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
        if (diff >  M_PI) diff -= 2.0f * M_PI;
        if (diff < -M_PI) diff += 2.0f * M_PI;
        *unwrapped += diff;
        *prev_a     = angle;
    }
    return *unwrapped;
}

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

  // 配置存在感探测器 (精细化高精度实验室模式)
  acc_detector_presence_config_start_set(presence_config, 0.2f);
  acc_detector_presence_config_end_set(presence_config, 1.2f);
  acc_detector_presence_config_automatic_subsweeps_set(presence_config, false); // 固定步长模式
  acc_detector_presence_config_signal_quality_set(presence_config, 60.0f);      // 极致信号质量
  acc_detector_presence_config_sweeps_per_frame_set(presence_config, 128);     // 单帧128次采样
  acc_detector_presence_config_frame_rate_set(presence_config, SAMPLE_RATE_HZ);
  acc_detector_presence_config_intra_detection_set(presence_config, true);
  acc_detector_presence_config_intra_detection_threshold_set(presence_config, 1.1f);
  acc_detector_presence_config_inter_detection_set(presence_config, true);
  acc_detector_presence_config_inter_detection_threshold_set(presence_config, 0.8f);

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

  printf("Entering main loop with Presence Detector (Phase Mode)...\n");

  // 粗扫-精测 状态机
  typedef enum { DET_SEARCHING, DET_COARSE, DET_MEASURING } det_phase_t;
  det_phase_t det_phase = DET_SEARCHING;

  int tracked_index = -1;
  int index         = -1;
  uint32_t phase_renorm_counter = 0;

  // 候选 bin（粗扫阶段同时追踪 tracked ±1 三个位置）
  int   candidate_bins[COARSE_MAX_CANDS];
  int   n_candidates = 0;
  float cand_prev_angle[COARSE_MAX_CANDS];
  float cand_unwrapped[COARSE_MAX_CANDS];
  bool  cand_first_phase[COARSE_MAX_CANDS];

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

    // --- 智能锁定追踪模式 (Smart Target Lock) ---
    // 目标：锁定最近的人体峰值，绝不因为微小扰动而切到墙上。

    // 必须与 presence_config 的 start/end 保持一致
    const float RANGE_START = 0.2f;
    const float RANGE_END   = 1.2f;

    if (result.depthwise_presence_scores_length < 2) {
      continue;
    }
    float step_length = (RANGE_END - RANGE_START) / (result.depthwise_presence_scores_length - 1);

    int prev_tracked = tracked_index;

    if (result.presence_detected) {
      missing_target_count = 0;
      if (tracked_index == -1) {
        // [搜索模式] 寻找全场最高能量点
        float max_s = 0.0f;
        for (uint32_t i = 0; i < result.depthwise_presence_scores_length; i++) {
          if (result.depthwise_inter_presence_scores[i] > max_s) {
            max_s = result.depthwise_inter_presence_scores[i];
            tracked_index = (int)i;
          }
        }
      } else {
        // [锁定模式] 仅在目标发生大距离漂移时才重新搜索
        float locked_dist = RANGE_START + tracked_index * step_length;
        if (fabsf(result.presence_distance - locked_dist) > 0.4f) {
           tracked_index = -1;
           ema_dist = 0.0f;
        }
      }
    } else {
      if (tracked_index != -1) {
        missing_target_count++;
        // Allow up to 10 seconds of "stillness" before dropping the target lock
        if (missing_target_count > (uint32_t)(10.0f * SAMPLE_RATE_HZ)) {
          tracked_index = -1;
          ema_dist = 0.0f;
          missing_target_count = 0;
        }
      } else {
        tracked_index = -1;
        ema_dist = 0.0f;
      }
    }

    uint16_t spf = acc_detector_presence_config_sweeps_per_frame_get(presence_config);

    // --- 状态转换：目标新出现 → 启动粗扫 ---
    if (prev_tracked == -1 && tracked_index != -1) {
      det_phase   = DET_COARSE;
      n_candidates = 0;
      int lo = tracked_index - 1;
      int hi = tracked_index + 1;
      for (int b = lo; b <= hi; b++) {
        if (b >= 0 && b < (int)result.depthwise_presence_scores_length) {
          candidate_bins[n_candidates]    = b;
          cand_prev_angle[n_candidates]   = 0.0f;
          cand_unwrapped[n_candidates]    = 0.0f;
          cand_first_phase[n_candidates]  = true;
          n_candidates++;
        }
      }
      vital_signs_coarse_start();
      printf("[System] Target appeared, starting coarse sweep (%d candidates)...\n", n_candidates);
    }

    // --- 状态转换：目标丢失 → 回到搜索 ---
    if (prev_tracked != -1 && tracked_index == -1) {
      vital_signs_init();
      first_phase           = true;
      phase_renorm_counter  = 0;
      det_phase             = DET_SEARCHING;
    }

    // --- 主状态机 ---
    if (tracked_index != -1) {
      float raw_dist   = RANGE_START + tracked_index * step_length;
      if (ema_dist == 0.0f) ema_dist = raw_dist;
      else ema_dist = 0.15f * raw_dist + 0.85f * ema_dist;
      float current_dist = ema_dist;

      if (det_phase == DET_COARSE) {
        // 并行更新所有候选 bin 的相位，喂入粗扫缓冲
        for (int c = 0; c < n_candidates; c++) {
          float ua = update_phase(&result, &metadata, spf, candidate_bins[c],
                                  &cand_prev_angle[c], &cand_first_phase[c], &cand_unwrapped[c]);
          vital_signs_coarse_feed(c, ua);
        }
        // 每帧计数一次，满 COARSE_N 帧后评选
        if (vital_signs_coarse_tick()) {
          int best = vital_signs_coarse_pick_best(n_candidates);
          if (best >= 0) {
            index         = candidate_bins[best];
            tracked_index = index;
            // 精测初始化：回放粗扫数据热启动滤波器，减少等待时间
            vital_signs_init();
            vital_signs_replay_coarse(best);
            // 继承粗扫阶段的相位状态，保证连续性
            prev_angle      = cand_prev_angle[best];
            unwrapped_angle = cand_unwrapped[best];
            first_phase     = false;
            phase_renorm_counter = 0;
            det_phase       = DET_MEASURING;
            printf("[System] Coarse sweep done! Best dist %.2fm, switching to fine mode\n",
                   RANGE_START + index * step_length);
          } else {
            printf("[System] Coarse sweep failed to find vital signs, searching again...\n");
            tracked_index = -1;
            ema_dist      = 0.0f;
            det_phase     = DET_SEARCHING;
          }
        }

      } else if (det_phase == DET_MEASURING) {
        index = tracked_index;

        // 定期重置相位基准，防止 float 精度长时间退化
        if (++phase_renorm_counter >= (uint32_t)(600.0f * SAMPLE_RATE_HZ)) {
          first_phase = true;
          vital_signs_init();
          phase_renorm_counter = 0;
        }

        update_phase(&result, &metadata, spf, index,
                     &prev_angle, &first_phase, &unwrapped_angle);

        printf("[LOCK] Dist: %.3fm | Intra: %.1f | Angle: %.2f\n",
               current_dist, result.intra_presence_score, unwrapped_angle);

        process_vital_signs(unwrapped_angle, current_dist);
        process_fall_detection(result.intra_presence_score, current_dist);
      }

    } else {
      if (acc_integration_get_time() % 2000 < 50) {
        printf("[System] Searching for target...\n");
      }
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
