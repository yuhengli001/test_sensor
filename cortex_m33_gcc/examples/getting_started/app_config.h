// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>


#define SAMPLE_RATE_HZ 20.0f
#define FFT_N          512


typedef enum
{
	MODE_NORMAL,    // Normal monitoring; no fall event detected
	MODE_SUSPECTED, // Post-impact phase; tracking whether subject stays at fallen position
	MODE_ALARM,     // Fall confirmed; immediate response required
} system_mode_t;


typedef struct
{
	// intra_score threshold to detect a sudden impact
	float fall_score_threshold;

	// Minimum distance change from pre-fall position to count as "fallen" (meters).
	// When the subject was resting before impact, 2x this value is required
	// to distinguish a fall from a bed roll or posture shift.
	float fall_dist_threshold;

	// Seconds the subject must remain at the fallen position to confirm the fall.
	// Does NOT require stillness — movement at the fallen position still counts.
	uint32_t confirm_period_sec;

	bool enable_fall_detection;
	bool enable_vitals_monitoring;
} app_config_t;


extern app_config_t  global_config;
extern system_mode_t sys_mode;


#endif // APP_CONFIG_H_
