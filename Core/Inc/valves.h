#pragma once
// =============================================================================
//  valves.h — EXV stepper + Octovalve driver
//  Ported from Superman-Firmware valves.cpp (Wim Boone, GPL-3.0)
// =============================================================================
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// EXV position: 0=closed, 255=fully open (matches original uint8_t pos)
void Valve_ExpansionSetPos(uint8_t valve, uint8_t new_pos);  // valve 1-6
void Valve_ExpansionRunSteps(void);    // call from TIM6 ISR (every 1 ms)
void Valve_ExpansionCalibrateAll(void);

void Valve_CoolantCondensorOpen(void);
void Valve_CoolantCondensorClose(void);

int  Valve_OctoSetPos(int target_pos); // 1-5, 0=stop
int  Valve_OctoGetPos(void);
void Valve_OctoRunTask(void);          // call every 10 ms
void Valve_OctoCalibrate(void);

// Called from EXTI ISR
void Valve_OctoISR(void);

// Turning direction (used by ISR)
extern bool g_valve_turning_direction;
extern bool g_octo_calibrating;
