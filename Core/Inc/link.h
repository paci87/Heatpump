#pragma once
// JSON-line telemetry / setpoints over USART1 (115200 8N1, PA9 TX / PA10 RX)

#include <stdint.h>

void Link_Init(void);
void Link_Process(void);  // call at 100 ms from Ms100Task
