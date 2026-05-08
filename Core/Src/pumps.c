// pumps.c — Waterpump PWM + RPM feedback
// Ported from Superman-Firmware pumps.cpp (Wim Boone, GPL-3.0)
// TIM4 CH1/CH2: PWM output (PB6/PB7), 10 Hz
// TIM4 CH3/CH4: Input capture RPM feedback (PB8/PB9)

#include "main.h"
#include "pumps.h"
#include "params.h"
#include "config.h"

volatile uint32_t g_pump_batt_period = 0;
volatile uint32_t g_pump_pt_period   = 0;

static TIM_HandleTypeDef *_htim4 = NULL;

// ─── Init ─────────────────────────────────────────────────────────────────────
void Pumps_Init(TIM_HandleTypeDef *htim4) {
    _htim4 = htim4;
    HAL_TIM_PWM_Start(htim4, PUMP_BATT_TIM_CH);
    HAL_TIM_PWM_Start(htim4, PUMP_PT_TIM_CH);
    HAL_TIM_IC_Start_IT(htim4, TIM_CHANNEL_3);  // battery feedback
    HAL_TIM_IC_Start_IT(htim4, TIM_CHANNEL_4);  // powertrain feedback
    // Start with 0% duty
    Pumps_BatterySetDuty(0);
    Pumps_PowertrainSetDuty(0);
}

// ─── PWM duty (0-80%, driver limit) ──────────────────────────────────────────
static void _setPumpDuty(uint32_t channel, uint8_t duty_pct) {
    if (!_htim4) return;
    if (duty_pct > PUMP_MAX_DUTY_PCT) duty_pct = PUMP_MAX_DUTY_PCT;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(_htim4);
    uint32_t ccr = (uint32_t)((float)duty_pct / 100.0f * (float)(arr + 1));
    __HAL_TIM_SET_COMPARE(_htim4, channel, ccr);
}

void Pumps_BatterySetDuty(uint8_t duty_pct) {
    Params_SetInt(PARAM_pump_battery_duty, duty_pct);
    _setPumpDuty(PUMP_BATT_TIM_CH, duty_pct);
}

void Pumps_PowertrainSetDuty(uint8_t duty_pct) {
    Params_SetInt(PARAM_pump_powertrain_duty, duty_pct);
    _setPumpDuty(PUMP_PT_TIM_CH, duty_pct);
}

// ─── RPM from period ticks (from pumps.cpp: 30,000,000 / period) ─────────────
float Pumps_BatteryGetFlow(void) {
    if (g_pump_batt_period == 0) return 0.0f;
    return PUMP_RPM_FROM_TICKS(g_pump_batt_period);
}

float Pumps_PowertrainGetFlow(void) {
    if (g_pump_pt_period == 0) return 0.0f;
    return PUMP_RPM_FROM_TICKS(g_pump_pt_period);
}

// ─── TIM4 input capture ISR — call from HAL_TIM_IC_CaptureCallback ────────────
// Measures period between rising edges on CH3 (battery) and CH4 (powertrain)
// TIM4 runs at 500 kHz (2µs/tick), period = 50000 ticks @ 10 Hz
void Pumps_TIM4_CaptureISR(TIM_HandleTypeDef *htim) {
    const uint32_t TIMER_PERIOD = 50000;
    static uint32_t last_batt = 0, last_pt = 0;

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {  // Battery pump CH3 PB8
        uint32_t now = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
        if (last_batt != 0) {
            g_pump_batt_period = (now >= last_batt)
                ? now - last_batt
                : (TIMER_PERIOD - last_batt) + now;
        }
        last_batt = now;
    }

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {  // Powertrain pump CH4 PB9
        uint32_t now = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
        if (last_pt != 0) {
            g_pump_pt_period = (now >= last_pt)
                ? now - last_pt
                : (TIMER_PERIOD - last_pt) + now;
        }
        last_pt = now;
    }
}
