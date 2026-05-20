#include "fan.h"
#include "params.h"
#include "config.h"
#include "tim.h"

extern TIM_HandleTypeDef htim2;

void Fan_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, FAN_TIM_CH);
    Fan_SetRadiatorDuty(0);
}

void Fan_SetRadiatorDuty(uint8_t duty_pct)
{
    if (duty_pct > 100) {
        duty_pct = 100;
    }
    Params_SetFloat(PARAM_radiatorfan_pwm, (float)duty_pct);
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim2);
    uint32_t ccr = (uint32_t)((float)duty_pct / 100.0f * (float)(arr + 1));
    __HAL_TIM_SET_COMPARE(&htim2, FAN_TIM_CH, ccr);
}
