#pragma once
// pumps.h — Waterpump PWM output + RPM feedback (TIM4)
#include <stdint.h>
#include "stm32g4xx_hal.h"

void  Pumps_Init(TIM_HandleTypeDef *htim4);
void  Pumps_BatterySetDuty(uint8_t duty_pct);    // 0-80% (driver limit)
void  Pumps_PowertrainSetDuty(uint8_t duty_pct);
float Pumps_BatteryGetFlow(void);    // RPM (approx)
float Pumps_PowertrainGetFlow(void); // RPM (approx)

// TIM4 capture ISR — call from HAL_TIM_IC_CaptureCallback
void Pumps_TIM4_CaptureISR(TIM_HandleTypeDef *htim);

// Volatile feedback values updated by ISR
extern volatile uint32_t g_pump_batt_period;
extern volatile uint32_t g_pump_pt_period;
