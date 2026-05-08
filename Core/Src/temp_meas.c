#include "temp_meas.h"
#include <stdint.h>

// =============================================================================
//  Tesla 10K NTC lookup table — ported directly from libopeninv temp_meas.cpp
//  Copyright (C) 2010 Johannes Huebner, Edward Cheeseman, Uwe Hermann (GPL-3.0)
//  Range: -30°C to +110°C, step 2°C, 71 entries
//  ADC values decrease as temperature increases (NTC behaviour)
//  Table entry: ADC raw value at each 2°C step starting from -30°C
// =============================================================================

static const uint16_t tesla10k[] = {
    4005, 3989, 3970, 3949, 3925, 3897, 3866, 3831, 3791, 3747,
    3698, 3643, 3583, 3518, 3446, 3369, 3286, 3198, 3104, 3005,
    2902, 2794, 2683, 2570, 2455, 2338, 2222, 2105, 1990, 1877,
    1766, 1658, 1554, 1454, 1358, 1267, 1180, 1098, 1020,  947,
     879,  815,  755,  700,  648,  600,  556,  515,  477,  441,
     409,  379,  351,  325,  302,  280,  260,  241,  224,  208,
     194,  180,  168,  157,  146,  136,  127,  119,  111,  104,  97
};

#define TESLA10K_TEMP_MIN  (-30)
#define TESLA10K_TEMP_MAX  (110)
#define TESLA10K_STEP      (2)
#define TESLA10K_LEN       (sizeof(tesla10k) / sizeof(tesla10k[0]))

// NTC: ADC value DECREASES as temperature INCREASES
// Find the first table entry <= digit (as temp rises, ADC falls)
float TempMeas_Lookup(uint16_t adcVal, TempCurve_t curve) {
    (void)curve;  // only TEMP_TESLA_10K supported here
    uint16_t last = tesla10k[0];

    for (uint32_t i = 0; i < TESLA10K_LEN; i++) {
        uint16_t cur = tesla10k[i];
        if (cur <= adcVal) {
            if (i == 0) return (float)TESLA10K_TEMP_MIN;
            // Interpolate between [i-1] and [i]
            float a = (float)(last - adcVal);
            float b = (float)(last - cur);
            float offset = (float)TESLA10K_STEP * a / b;
            float base   = (float)TESLA10K_TEMP_MIN + (float)(TESLA10K_STEP * (int)i);
            return base - offset;
        }
        last = cur;
    }
    return (float)TESLA10K_TEMP_MAX;
}
