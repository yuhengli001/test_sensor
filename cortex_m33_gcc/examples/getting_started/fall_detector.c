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
            printf("\n[APP NOTIFY] ⚠️ 检测到疑似跌倒撞击！\n");
        }
      } else {
        impact_frame_cnt = 0;
      }
      break;

    case MODE_SUSPECTED:
      if (intra_score < 1.5f) {
        suspected_frame_cnt++;
        if (suspected_frame_cnt >= (uint32_t)(global_config.confirm_period_sec * SAMPLE_RATE_HZ)) {
          if (current_dist > pre_fall_dist + 0.3f) {
              sys_mode = MODE_ALARM;
          } else {
              printf("[APP NOTIFY] ✅ 误报撤回：位置未变。\n");
              sys_mode = MODE_NORMAL;
          }
        }
      } else if (intra_score > 10.0f) {
        printf("[APP NOTIFY] 🏃 运动恢复，报警取消。\n");
        sys_mode = MODE_NORMAL;
      } else {
        // 中等运动 (1.5 ~ 10.0)：重置静止计时器，不确认也不取消
        suspected_frame_cnt = 0;
      }
      break;

    case MODE_ALARM:
      // 报警保持锁存，每 2 秒打印一次，直到 fall_detector_reset_alarm() 被调用
      if (alarm_print_counter++ >= (uint32_t)(2.0f * SAMPLE_RATE_HZ)) {
          printf("\n!!! [CRITICAL] 确认跌倒发生 - 等待处理 !!!\n");
          alarm_print_counter = 0;
      }
      break;
  }

  FALL_APP_UpdateData((uint8_t)sys_mode, 0.0f, current_dist);
}
