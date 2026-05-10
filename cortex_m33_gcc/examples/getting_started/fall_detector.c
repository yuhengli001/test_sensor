#include "fall_detector.h"
#include <stdio.h>
#include <math.h>
#include "acc_definitions_a121.h"
#include "acc_integration_log.h"

app_config_t global_config = {
    .fall_vel_threshold = 1.6f,
    .still_threshold = 0.02f,
    .confirm_period_sec = 10,
    .enable_fall_detection = true,
    .enable_vitals_monitoring = true,
};

system_mode_t sys_mode = MODE_NORMAL;
static uint32_t suspected_frame_cnt = 0;

void fall_detector_init(void) {
    sys_mode = MODE_NORMAL;
    suspected_frame_cnt = 0;
}

void process_fall_detection(float velocity, float difference, float current_dist) {
  if (!global_config.enable_fall_detection) return;

  switch (sys_mode) {
    case MODE_NORMAL:
      if (fabsf(velocity) > global_config.fall_vel_threshold) {
        sys_mode = MODE_SUSPECTED;
        suspected_frame_cnt = 0;
        printf("\n[FALL DETECTOR] Impact detected! Vel: %" PRIfloat " m/s\n",
               ACC_LOG_FLOAT_TO_INTEGER(velocity));
      }
      break;

    case MODE_SUSPECTED:
      if (fabsf(difference) < global_config.still_threshold) {
        suspected_frame_cnt++;
        if (suspected_frame_cnt % 40 == 0) {
          printf("[FALL DETECTOR] Checking stillness... %d/%ds\n", 
                 (int)(suspected_frame_cnt / SAMPLE_RATE_HZ), (int)global_config.confirm_period_sec);
        }
        if (suspected_frame_cnt >= (global_config.confirm_period_sec * SAMPLE_RATE_HZ)) {
          sys_mode = MODE_ALARM;
        }
      } else if (fabsf(velocity) > 0.6f) {
        printf("[FALL DETECTOR] Movement recovery. Resetting.\n");
        sys_mode = MODE_NORMAL;
      }
      break;

    case MODE_ALARM:
      printf("\n*******************************************\n");
      printf("*** ALERT: FALL CONFIRMED AT %" PRIfloat "m ***\n", ACC_LOG_FLOAT_TO_INTEGER(current_dist));
      printf("*******************************************\n");
      sys_mode = MODE_NORMAL; 
      break;
  }
}
