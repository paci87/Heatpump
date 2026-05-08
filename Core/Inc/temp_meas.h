#pragma once
// =============================================================================
//  temp_meas.h — Tesla 10K NTC temperature lookup (from Superman-Firmware)
//  Uses TEMP_TESLA_10K curve from libopeninv TempMeas class
// =============================================================================
#include <stdint.h>

typedef enum {
    TEMP_TESLA_10K = 5,  // matches TempMeas::TEMP_TESLA_10K from libopeninv
} TempCurve_t;

// Lookup: ADC raw value (0-4095) → temperature in °C (×10 fixed point internally,
// returned as float)
float TempMeas_Lookup(uint16_t adcVal, TempCurve_t curve);
