#ifndef FALL_DETECTOR_H_
#define FALL_DETECTOR_H_

#include "app_config.h"

void fall_detector_init(void);
void process_fall_detection(float velocity, float difference, float current_dist);

#endif
