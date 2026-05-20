#pragma once

#include <stdint.h>

void Fan_Init(void);
void Fan_SetRadiatorDuty(uint8_t duty_pct);  // TIM2 CH4 PB11, 0-100 %
