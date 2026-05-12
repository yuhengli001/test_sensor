#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

#define SAMPLE_RATE_HZ 20.0f
#define FFT_N          512

typedef enum {
  MODE_NORMAL,
  MODE_SUSPECTED,
  MODE_ALARM
} system_mode_t;

typedef struct {
  float fall_vel_threshold;
  float still_threshold;
  uint32_t confirm_period_sec;
  bool enable_fall_detection;
  bool enable_vitals_monitoring;
} app_config_t;

extern app_config_t global_config;
extern system_mode_t sys_mode;

#endif
