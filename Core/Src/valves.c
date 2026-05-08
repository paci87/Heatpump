//  valves.c — EXV + Octovalve driver for STM32G474VET6
//  Ported from Superman-Firmware valves.cpp (Wim Boone, GPL-3.0)
//  Hardware abstraction changed: libopencm3 DigIo → STM32 HAL GPIO

#include "main.h"
#include "valves.h"
#include "params.h"
#include "config.h"
#include "stm32g4xx_hal.h"
#include <stdlib.h>
#include <string.h>

// ─── EXV GPIO lookup tables (matches digio_prj.h order) ──────────────────────
static const GpioPin_t STEP_PINS[EXV_COUNT] = EXV_STEP_PINS;
static const GpioPin_t DIR_PINS [EXV_COUNT] = EXV_DIR_PINS;
static const GpioPin_t EN_PINS  [EXV_COUNT] = EXV_EN_PINS;

// Parameter IDs for EXV position tracking (matches param_prj.h VALUE_ENTRY order)
static const ParamId_t EXV_PARAM_IDS[EXV_COUNT] = {
    PARAM_expv_condensor_coolant,    // EXV1
    PARAM_expv_condensor_cabinl,     // EXV2
    PARAM_expv_recirculation,        // EXV3
    PARAM_expv_condensor_cabinr,     // EXV4
    PARAM_expv_evaporator_coolant,   // EXV5
    PARAM_expv_chiller,     // EXV6
};

// Step queues — steps remaining per valve (decremented in ISR)
static volatile int steps_queue[EXV_COUNT] = {0};
static volatile bool calibration_active = false;
static volatile bool step_high[EXV_COUNT] = {false}; // pulse high/low state

// ─── Octovalve state ──────────────────────────────────────────────────────────
volatile int g_octovalve_pulse_count = 0;  // updated by EXTI ISR
bool         g_valve_turning_direction = CLOCKWISE;
bool         g_octo_calibrating = false;

static int   octo_target_position = 0;
static int   octo_current_position = 0;
static uint32_t last_pulse_time_ms = 0;
static int   last_pulse_count = 0;

static const int pos_centers[6] = {0, 0, 240, 450, 670, 950}; // index 0 unused

// ─── EXV helpers ──────────────────────────────────────────────────────────────
static inline void _exvStep(uint8_t idx, GPIO_PinState state) {
    HAL_GPIO_WritePin(STEP_PINS[idx].port, STEP_PINS[idx].pin, state);
}
static inline void _exvDir(uint8_t idx, bool open) {
    HAL_GPIO_WritePin(DIR_PINS[idx].port, DIR_PINS[idx].pin,
                      open ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
static inline void _exvEnable(uint8_t idx, bool en) {
    // active LOW
    HAL_GPIO_WritePin(EN_PINS[idx].port, EN_PINS[idx].pin,
                      en ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

// ─── EXV: run steps — called from TIM6 ISR every 1 ms ─────────────────────────
//  Matches expansionRunSteps() from valves.cpp exactly.
//  Toggle STEP pin: high → valve steps, then next call sets it low.
void Valve_ExpansionRunSteps(void) {
    for (int i = 0; i < EXV_COUNT; i++) {
        if (steps_queue[i] > 0) {
            // Toggle step pin
            if (step_high[i]) {
                _exvStep(i, GPIO_PIN_RESET);
                step_high[i] = false;
                steps_queue[i]--;   // count step on falling edge (matches Toggle logic)
            } else {
                _exvStep(i, GPIO_PIN_SET);
                step_high[i] = true;
            }
        } else {
            _exvStep(i, GPIO_PIN_RESET);
            step_high[i] = false;
            _exvEnable(i, false);  // Disable driver when idle
            if (calibration_active) calibration_active = false;
        }
    }
}

// ─── EXV: set position (0=closed, 255=open) ────────────────────────────────────
void Valve_ExpansionSetPos(uint8_t valve, uint8_t new_pos) {
    if (valve < 1 || valve > EXV_COUNT) return;
    uint8_t idx = valve - 1;
    if (calibration_active || steps_queue[idx] != 0) return;

    uint8_t current_pos = (uint8_t)Params_GetInt(EXV_PARAM_IDS[idx]);

    if (new_pos > current_pos) {
        _exvEnable(idx, true);
        _exvDir(idx, true);        // high = opening
        steps_queue[idx] = new_pos - current_pos;
        Params_SetInt(EXV_PARAM_IDS[idx], new_pos);
    } else if (new_pos < current_pos) {
        _exvEnable(idx, true);
        _exvDir(idx, false);       // low = closing
        steps_queue[idx] = current_pos - new_pos;
        Params_SetInt(EXV_PARAM_IDS[idx], new_pos);
    }
}

// ─── EXV: calibrate all to closed ─────────────────────────────────────────────
void Valve_ExpansionCalibrateAll(void) {
    calibration_active = true;
    for (int i = 0; i < EXV_COUNT; i++) {
        steps_queue[i] = 0;
        _exvEnable(i, true);
        _exvDir(i, false);         // close direction
        steps_queue[i] = 255;
        Params_SetInt(EXV_PARAM_IDS[i], 0);
    }
}

// ─── LCC coolant solenoid (direct GPIO for simple open/close) ─────────────────
void Valve_CoolantCondensorOpen(void) {
    HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_RESET);
    Params_SetInt(PARAM_valve_coolant_condensor, 0);
}

void Valve_CoolantCondensorClose(void) {
    HAL_GPIO_WritePin(SOLENOID_PORT, SOLENOID_PIN, GPIO_PIN_SET);
    Params_SetInt(PARAM_valve_coolant_condensor, 1);
}

// ─── Octovalve: EXTI pulse ISR ────────────────────────────────────────────────
void Valve_OctoISR(void) {
    if (g_valve_turning_direction == CLOCKWISE) {
        g_octovalve_pulse_count++;
    } else {
        g_octovalve_pulse_count--;
    }
}

// ─── Octovalve: set target position ──────────────────────────────────────────
int Valve_OctoSetPos(int set_position) {
    if (g_octo_calibrating) return -1;
    if (set_position < 0 || set_position > 5) return -1;
    octo_target_position = set_position;
    Params_SetInt(PARAM_octovalve_setpoint, set_position);
    return 0;
}

int Valve_OctoGetPos(void) { return octo_current_position; }

// ─── Octovalve: run task — call every 10 ms ───────────────────────────────────
//  Direct port of octoRunTask() from valves.cpp
void Valve_OctoRunTask(void) {
    const int  POS_TOL    = OCTO_POSITION_TOLERANCE;
    const uint32_t STALL  = 20;  // 20 × 10ms = 200ms stall timeout

    int current_pulses = g_octovalve_pulse_count;

    // Compute closest position from pulse count
    int closest_pos = 1;
    int min_error   = abs(current_pulses - pos_centers[1]);
    for (int pos = 2; pos <= 5; pos++) {
        int error = abs(current_pulses - pos_centers[pos]);
        if (error < min_error) { min_error = error; closest_pos = pos; }
    }
    octo_current_position = closest_pos;
    Params_SetInt(PARAM_octovalve_position, octo_current_position);

    uint32_t now_ms = HAL_GetTick();

    // Calibration mode
    if (g_octo_calibrating) {
        if (current_pulses != last_pulse_count) {
            last_pulse_count = current_pulses;
            last_pulse_time_ms = now_ms;
        } else {
            if ((now_ms - last_pulse_time_ms) >= (STALL * 10)) {
                HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_RESET);
                g_octovalve_pulse_count = 0;
                g_octo_calibrating = false;
            }
        }
        return;
    }

    // No target
    if (octo_target_position == 0) {
        HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_RESET);
        last_pulse_count   = current_pulses;
        last_pulse_time_ms = now_ms;
        return;
    }

    int target_pulses  = pos_centers[octo_target_position];
    int position_error = target_pulses - current_pulses;

    if (abs(position_error) > POS_TOL) {
        // Check for stall
        if (current_pulses != last_pulse_count) {
            last_pulse_count   = current_pulses;
            last_pulse_time_ms = now_ms;
        } else if ((now_ms - last_pulse_time_ms) >= (STALL * 10)) {
            // Stalled — correct endstop positions
            HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_RESET);
            if (octo_target_position == 1) g_octovalve_pulse_count = pos_centers[1];
            if (octo_target_position == 5) g_octovalve_pulse_count = pos_centers[5];
            octo_target_position = 0;
            return;
        }

        // Drive motor
        if (position_error > 0) {  // CW
            g_valve_turning_direction = CLOCKWISE;
            HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_RESET);
        } else {                   // CCW
            g_valve_turning_direction = COUNTERCLOCKWISE;
            HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_SET);
        }
    } else {
        // Position reached — stop
        HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_RESET);
        last_pulse_count   = current_pulses;
        last_pulse_time_ms = now_ms;
    }
}

// ─── Octovalve: start calibration (CCW to endstop) ────────────────────────────
void Valve_OctoCalibrate(void) {
    g_octo_calibrating = true;
    octo_target_position = 0;
    g_valve_turning_direction = COUNTERCLOCKWISE;
    HAL_GPIO_WritePin(OCTO_IN1_PORT, OCTO_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OCTO_IN2_PORT, OCTO_IN2_PIN, GPIO_PIN_SET);
    last_pulse_time_ms = HAL_GetTick();
    last_pulse_count   = g_octovalve_pulse_count;
}
