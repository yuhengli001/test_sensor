#ifndef FALL_DETECTOR_H_
#define FALL_DETECTOR_H_

#include "app_config.h"

void fall_detector_init(void);
void fall_detector_reset_alarm(void);
void process_fall_detection(float intra_score, float current_dist);

#endif
