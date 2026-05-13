#include "radar_adapter.h"
#include "acc_hal_definitions_a121.h"
#include "acc_hal_integration_a121.h"
#include "acc_rss_a121.h"
#include "acc_integration.h"
#include "acc_sensor.h"
#include "acc_processing.h"
#include "acc_config.h"
#include "log_module.h"

/* Vibration Example Includes */
#include "example_vibration.h"
#include "vibration_service_app.h"

/* Presence (Vital/Fall) Includes */
#include "acc_detector_presence.h"
#include "fall_detector.h"
#include "vital_signs.h"
#include <complex.h>
#include <math.h>

#ifdef __PACKED_STRUCT
#error "__PACKED_STRUCT IS DEFINED!"
#else
#error "__PACKED_STRUCT IS NOT DEFINED!"
#endif
