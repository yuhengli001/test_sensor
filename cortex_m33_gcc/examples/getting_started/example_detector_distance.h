// Copyright (c) Acconeer AB, 2022
// All rights reserved
// This file is subject to the terms and conditions defined in the file
// 'LICENSES/license_acconeer.txt', (BSD 3-Clause License) which is part
// of this source code package.

#ifndef EXAMPLE_DETECTOR_DISTANCE_H_
#define EXAMPLE_DETECTOR_DISTANCE_H_

#include <stdbool.h>

/**
 * @brief Combined init + run loop.  Never returns in normal operation.
 *        Kept for builds that do not need BLE co-operation.
 */
int acc_example_detector_distance(int argc, char *argv[]);

/**
 * @brief Initialise the presence detector and all sub-modules.
 *        Must be called once before acc_example_detector_distance_step().
 * @retval EXIT_SUCCESS on success, EXIT_FAILURE on any hardware or allocation error.
 */
int acc_example_detector_distance_init(void);

/**
 * @brief Process one radar frame.
 *        Call repeatedly from the main loop alongside MX_APPE_Process().
 *        Updates g_fall_status, g_presence_dist and g_presence_valid (fall_detector.h).
 * @retval true  Frame processed normally (or skipped for calibration).
 * @retval false Unrecoverable sensor error; resources have been released.
 */
bool acc_example_detector_distance_step(void);

/**
 * @brief Release all sensor and detector resources.
 */
void acc_example_detector_distance_deinit(void);

#endif
