#pragma once
// =============================================================================
//  thermal_control.h — Full thermal management logic
//  Ported from Superman-Firmware thermal_control.cpp (Wim Boone, GPL-3.0)
//
//  thermalControl() is the top-level function called every 100 ms.
//  It:
//    1. Assesses thermal demands from sensor readings and user inputs
//    2. Selects best heat source and sink
//    3. Sets octovalve position for the correct coolant routing
//    4. Runs PI controller on compressor duty (condenser or evaporator setpoint)
//    5. Opens/closes EXVs for flow distribution
//    6. Sets pump duties proportional to compressor load
// =============================================================================

void thermalControl(void);
