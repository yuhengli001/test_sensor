// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#ifndef FALL_DETECTOR_H_
#define FALL_DETECTOR_H_

#include "app_config.h"


/**
 * @brief Status codes returned by process_fall_detection() each frame.
 *
 *   1 (NORMAL)    - No event; normal monitoring in progress.
 *   2 (IMPACT)    - Sustained energy burst detected; tracking frames toward suspicion.
 *   3 (SUSPECTED) - Post-impact stillness phase; confirming whether a fall occurred.
 *   4 (ALARM)     - Fall confirmed; immediate response required.
 */
typedef enum
{
	FALL_STATUS_NORMAL    = 1,
	FALL_STATUS_IMPACT    = 2,
	FALL_STATUS_SUSPECTED = 3,
	FALL_STATUS_ALARM     = 4,
} fall_status_t;


/**
 * @brief Reset all fall detector state to NORMAL.
 */
void fall_detector_init(void);


/**
 * @brief Clear an active alarm and return to NORMAL mode.
 */
void fall_detector_reset_alarm(void);


/**
 * @brief Run one frame of fall detection logic.
 *
 * @param intra_score   Intra-frame presence score from the presence detector.
 * @param current_dist  Current estimated subject distance in meters.
 * @retval fall_status_t  Current fall status after processing this frame (see enum above).
 */
fall_status_t process_fall_detection(float intra_score, float current_dist);


// -------------------------------------------------------------------------
// Shared output state — written by the presence detector step function,
// read by the BLE notification layer to build the outgoing packet.
// -------------------------------------------------------------------------

/** Current fall detection state. Updated every frame by process_fall_detection(). */
extern fall_status_t g_fall_status;

/** Latest smoothed subject distance in metres. Valid only when g_presence_valid is true. */
extern float g_presence_dist;

/** True once the presence detector has acquired a target lock and g_presence_dist is valid. */
extern bool g_presence_valid;


#endif // FALL_DETECTOR_H_
