// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#ifndef VITAL_SIGNS_H_
#define VITAL_SIGNS_H_

#include "app_config.h"


// Number of frames collected during the coarse sweep (6.4 s at 20 Hz; power-of-two for FFT)
#define COARSE_N         128

// Maximum number of candidate range bins evaluated in parallel during the coarse sweep
#define COARSE_MAX_CANDS 64


// --- Fine measurement interface ---

/**
 * @brief Reset all vital-signs filter state. Must be called before first use or after a lock reset.
 */
void vital_signs_init(void);

/**
 * @brief Process one frame of vital-signs data.
 * @param difference    Unwrapped phase angle from the locked range bin (radians).
 * @param current_dist  Current subject distance estimate in meters.
 */
void process_vital_signs(float difference, float current_dist);


// --- Coarse sweep interface ---

/**
 * @brief Start a new coarse sweep; resets all coarse buffers.
 */
void vital_signs_coarse_start(void);

/**
 * @brief Feed one frame of unwrapped phase into the coarse sweep buffer for a given slot.
 * @param slot   Candidate bin index (0 to COARSE_MAX_CANDS-1).
 * @param angle  Unwrapped phase value for this frame.
 */
void vital_signs_coarse_feed(int slot, float angle);

/**
 * @brief Advance the coarse sweep frame counter by one.
 * @retval true when COARSE_N frames have been collected and evaluation can begin.
 */
bool vital_signs_coarse_tick(void);

/**
 * @brief Evaluate all candidate bins and return the best one.
 * @param n_candidates  Number of active candidate slots.
 * @retval Index of the best slot, or -1 if no viable candidate was found.
 */
int vital_signs_coarse_pick_best(int n_candidates);

/**
 * @brief Replay coarse sweep data into the fine filter to reduce the initial warm-up delay.
 * @param best_slot  Slot index returned by vital_signs_coarse_pick_best().
 */
void vital_signs_replay_coarse(int best_slot);


#endif // VITAL_SIGNS_H_
