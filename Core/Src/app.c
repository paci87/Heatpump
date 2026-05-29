#include "main.h"
#include "params.h"
#include "valves.h"
#include "sensors.h"
#include "pumps.h"
#include "can_comp.h"
#include "thermal_control.h"

extern ADC_HandleTypeDef  hadc1;
extern ADC_HandleTypeDef  hadc2;
extern FDCAN_HandleTypeDef hfdcan1;
extern TIM_HandleTypeDef  htim4;
extern TIM_HandleTypeDef  htim6;
extern TIM_HandleTypeDef  htim7;
extern IWDG_HandleTypeDef hiwdg;

static volatile uint32_t s_tick10  = 0;
static volatile uint32_t s_tick100 = 0;
static volatile uint32_t s_tick200 = 0;

// ─── ADC-based octovalve encoder (temporary — replace with transistor) ─────────
// PA3 = ADC1 rank 4 = g_adc1Dma[3], threshold midpoint of 0-82 range
#define OCTO_ADC_THRESHOLD  40

static void Ms1Task(void)   { Valve_ExpansionRunSteps(); Valve_OctoSensePoll(); }

static void Ms10Task(void) {
    Param_SetInt(heat_cabinl,      HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_10) == GPIO_PIN_SET ? 1 : 0);
    Param_SetInt(heat_cabinr,      HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_8)  == GPIO_PIN_SET ? 1 : 0);
    Param_SetInt(cool_cabin,       HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_9)  == GPIO_PIN_SET ? 1 : 0);
    Param_SetInt(preheat_req,      HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)  == GPIO_PIN_SET ? 1 : 0);
    Param_SetInt(reservoir_level,  HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1)  == GPIO_PIN_SET ? 1 : 0);
    Params_SetFloat(PARAM_pump_battery_flow,   Pumps_BatteryGetFlow());
    Params_SetFloat(PARAM_pump_powertrain_flow, Pumps_PowertrainGetFlow());
    Valve_OctoRunTask();
    Param_SetInt(octovalve_position, Valve_OctoGetPos());
}

static void Ms100Task(void) {
    HAL_IWDG_Refresh(&hiwdg);
    Params_SetFloat(PARAM_uaux, Sensors_GetUaux());
    Sensors_Update();
    CanComp_Update();
    thermalControl();
}

static void Ms200Task(void) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM8) { HAL_IncTick(); }
    if (htim->Instance == TIM6) { Ms1Task(); return; }
    if (htim->Instance == TIM7) {
        if (++s_tick10  >= 10)  { s_tick10  = 0; Ms10Task();  }
        if (++s_tick100 >= 100) { s_tick100 = 0; Ms100Task(); }
        if (++s_tick200 >= 200) { s_tick200 = 0; Ms200Task(); }
    }
}


void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) Pumps_TIM4_CaptureISR(htim);
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
    (void)RxFifo0ITs;
    CanComp_RxCallback(hfdcan);
}

void App_Init(void) {
    Params_Init();
    Sensors_Init(&hadc1, &hadc2);
    Pumps_Init(&htim4);
    CanComp_Init(&hfdcan1);
    HAL_TIM_Base_Start_IT(&htim6);
    HAL_TIM_Base_Start_IT(&htim7);   // ← IWDG now being refreshed
    Valve_ExpansionCalibrateAll();
    HAL_Delay(100);
    Valve_OctoCalibrate();
    uint32_t t = HAL_GetTick();
    while (g_octo_calibrating && (HAL_GetTick() - t) < 5000) {
        HAL_Delay(10);
        Valve_OctoRunTask();
        HAL_IWDG_Refresh(&hiwdg);    // ← add this to feed watchdog during long calibration
    }
    HAL_Delay(500);
    Valve_OctoSetPos(3);
}
