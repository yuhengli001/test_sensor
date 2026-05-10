#ifndef VITAL_SIGNS_H_
#define VITAL_SIGNS_H_

#include "app_config.h"

void vital_signs_init(void);
void process_vital_signs(float difference, float current_dist);

#endif
