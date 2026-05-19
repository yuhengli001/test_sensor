#include "fall_detector.h"
#include <stdio.h>
#include <math.h>
#include "acc_definitions_a121.h"
#include "acc_integration_log.h"
#include "main.h"
#include "fall_service_app.h"

app_config_t global_config = {
    .fall_score_threshold = 20.0f,
    .still_threshold = 0.02f,
    .confirm_period_sec = 10,
    .enable_fall_detection = true,
    .enable_vitals_monitoring = true,
};

system_mode_t sys_mode = MODE_NORMAL;

// --- 状态机内部计数器与标志 ---
static uint32_t suspected_frame_cnt      = 0;
static uint32_t impact_frame_cnt         = 0;
static uint32_t alarm_print_counter      = 0;
static uint32_t resting_frame_cnt        = 0;
static float    pre_fall_dist            = 0.0f;
static bool     was_resting_before_impact = false;

void fall_detector_init(void) {
    sys_mode = MODE_NORMAL;
    suspected_frame_cnt = 0;
    impact_frame_cnt = 0;
    alarm_print_counter = 0;
}

void fall_detector_reset_alarm(void) {
    if (sys_mode == MODE_ALARM) {
        sys_mode = MODE_NORMAL;
        impact_frame_cnt = 0;
        suspected_frame_cnt = 0;
        alarm_print_counter = 0;
        printf("[FALL] Alarm cleared\n");
    }
}

void process_fall_detection(float intra_score, float current_dist) {
  if (!global_config.enable_fall_detection) return;

  switch (sys_mode) {
    case MODE_NORMAL:
      if (intra_score > 20.0f) {
        if (impact_frame_cnt == 0) pre_fall_dist = current_dist;
        impact_frame_cnt++;
        if (impact_frame_cnt >= (uint32_t)(0.5f * SAMPLE_RATE_HZ)) {
            sys_mode = MODE_SUSPECTED;
            suspected_frame_cnt = 0;
            printf("\n[FALL] Suspected impact\n");
        }
      } else {
        // 改进：允许短暂单帧掉帧，每次仅递减 2 而非归零
        impact_frame_cnt = (impact_frame_cnt > 2) ? impact_frame_cnt - 2 : 0;
      }
      break;

    case MODE_SUSPECTED:
      // 爆发后的静止确认阶段 (Post-Impact Stillness)
      // 改进：阈值放宽到 5.0f，允许呼吸与微小动作
      if (intra_score < 5.0f) {
        suspected_frame_cnt++;
        if (suspected_frame_cnt >= (uint32_t)(global_config.confirm_period_sec * SAMPLE_RATE_HZ)) {

          float dist_diff = fabsf(current_dist - pre_fall_dist);

          if (was_resting_before_impact) {
              // 情景 A：本来在平躺或静坐 (呼吸监测中)
              // 必须伴随非常显著的距离变化（例如从床上滚落到地上），才算跌倒
              if (dist_diff > 0.6f) {
                  sys_mode = MODE_ALARM;
              } else {
                  printf("[FALL] Intercepted: Bed movement\n");
                  sys_mode = MODE_NORMAL;
                  // 继承之前的静止状态，防止连续翻身触发
                  resting_frame_cnt = (uint32_t)(5.0f * SAMPLE_RATE_HZ);
              }
          } else {
              // 情景 B：本来在走动/活动中
              // 突然的高速爆发 + 倒地不起。即使壁挂安装，水平距离也至少会有小幅滑动。
              if (dist_diff > 0.15f) {
                  sys_mode = MODE_ALARM;
              } else {
                  printf("[FALL] Intercepted: Sitting down\n");
                  sys_mode = MODE_NORMAL;
                  resting_frame_cnt = 0;
              }
          }
        }
      } else if (intra_score > 10.0f) {
        printf("[FALL] Cancelled: Motion detected\n");
        sys_mode = MODE_NORMAL;
      } else {
        // 改进：中等幅度运动 (5.0 ~ 10.0) 不直接清零，而是每次递减 1，允许短暂微动
        if (suspected_frame_cnt > 0) {
            suspected_frame_cnt--;
        }
      }
      break;

    case MODE_ALARM:
      // 报警保持锁存，每 2 秒打印一次，直到 fall_detector_reset_alarm() 被调用
      if (alarm_print_counter++ >= (uint32_t)(2.0f * SAMPLE_RATE_HZ)) {
          printf("\n[CRITICAL] Fall detected!\n");
          alarm_print_counter = 0;
      }
      break;
  }

  FALL_APP_UpdateData((uint8_t)sys_mode, 0.0f, current_dist);
}
