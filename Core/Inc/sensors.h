#pragma once
// sensors.h
#include <stdint.h>
#include "stm32g4xx_hal.h"

// Two separate DMA buffers — one per ADC
extern uint16_t g_adc1Dma[5];   // ADC1: PA0,PA1,PA2,PA3,PB0
extern uint16_t g_adc2Dma[8];   // ADC2: PA4,PA5,PC0-PC5

void  Sensors_Init(ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2);
void  Sensors_Update(void);
float Sensors_GetUaux(void);
