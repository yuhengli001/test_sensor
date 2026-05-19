#include "fall_detector.h"
#include <stdio.h>
#include <math.h>
#include "acc_definitions_a121.h"
#include "acc_integration_log.h"

app_config_t global_config = {
    .fall_score_threshold = 20.0f,
    .still_threshold = 0.02f,
    .confirm_period_sec = 10,
    .enable_fall_detection = true,
    .enable_vitals_monitoring = true,
};

system_mode_t sys_mode = MODE_NORMAL;
static uint32_t suspected_frame_cnt = 0;
static uint32_t impact_frame_cnt = 0;
static uint32_t alarm_print_counter = 0;
static float    pre_fall_dist    = 0.0f;

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
        printf("[APP NOTIFY] 跌倒报警已清除。\n");
    }
}

static uint32_t resting_frame_cnt = 0;
static bool was_resting_before_impact = false;

void process_fall_detection(float intra_score, float current_dist) {
  if (!global_config.enable_fall_detection) return;

  switch (sys_mode) {
    case MODE_NORMAL:
      // 追踪静止状态 (Resting State Tracking)
      if (intra_score < 5.0f) {
          resting_frame_cnt++;
      } else if (intra_score > 10.0f) {
          resting_frame_cnt = 0;
      }

      // 能量爆发检测 (Impact Detection)
      if (intra_score > 20.0f) {
        if (impact_frame_cnt == 0) {
            pre_fall_dist = current_dist;
            // 如果爆发前连续 5 秒处于安静状态，则认为原本是平躺/静坐 (Vitals Locked Immunity)
            was_resting_before_impact = (resting_frame_cnt > (uint32_t)(5.0f * SAMPLE_RATE_HZ));
        }
        impact_frame_cnt++;
        if (impact_frame_cnt >= (uint32_t)(0.5f * SAMPLE_RATE_HZ)) {
            sys_mode = MODE_SUSPECTED;
            suspected_frame_cnt = 0;
            printf("\n[APP NOTIFY] ⚠️ 检测到疑似动作爆发！\n");
        }
      } else {
        impact_frame_cnt = 0;
      }
      break;

    case MODE_SUSPECTED:
      // 爆发后的静止确认阶段 (Post-Impact Stillness)
      if (intra_score < 1.5f) {
        suspected_frame_cnt++;
        if (suspected_frame_cnt >= (uint32_t)(global_config.confirm_period_sec * SAMPLE_RATE_HZ)) {
          
          float dist_diff = fabsf(current_dist - pre_fall_dist);
          
          if (was_resting_before_impact) {
              // 情景 A：本来在平躺或静坐 (呼吸监测中)
              // 必须伴随非常显著的距离变化（例如从床上滚落到地上），才算跌倒
              if (dist_diff > 0.6f) {
                  sys_mode = MODE_ALARM;
              } else {
                  printf("[APP NOTIFY] ✅ 误报拦截：原静止状态下的姿态调整 (如翻身、伸懒腰)。\n");
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
                  printf("[APP NOTIFY] ✅ 误报拦截：位置无明显滑移，疑似快速坐下。\n");
                  sys_mode = MODE_NORMAL;
                  resting_frame_cnt = 0;
              }
          }
        }
      } else if (intra_score > 10.0f) {
        printf("[APP NOTIFY] 🏃 运动恢复，警报取消。\n");
        sys_mode = MODE_NORMAL;
        resting_frame_cnt = 0;
      } else {
        // 中等幅度运动 (1.5 ~ 10.0)：重置静止计时器，不确认也不取消
        suspected_frame_cnt = 0;
      }
      break;

    case MODE_ALARM:
      if (alarm_print_counter++ >= (uint32_t)(2.0f * SAMPLE_RATE_HZ)) {
          printf("\n!!! [CRITICAL] 确认跌倒发生 - 等待救援 !!!\n");
          alarm_print_counter = 0;
      }
      break;
  }
}
