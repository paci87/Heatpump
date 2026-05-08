#pragma once
// =============================================================================
//  can_comp.h — AC Compressor FDCAN driver (STM32G474VET6)
//  Ported from Superman-Firmware interface.cpp + pumps.cpp (Wim Boone, GPL-3.0)
//
//  REAL CAN IDs (verified from source):
//    TX 0x281 — VCFRONT command: duty (0.1% LE), power limit (W LE), enable
//    RX 0x227 — Model 3 compressor status: RPM, duty, temp, fault flags
//    RX 0x2A8 — HV status: RPM, duty, power, current, voltage, state
//    TX 0x732 — HP manifold telemetry 1 (temps, pressures, pump flows)
//    TX 0x733 — HP manifold telemetry 2 (temps, fan, octovalve, comp speed)
//    RX 0x730 — Param updates (CAN-IO)
//    RX 0x731 — Temperature setpoints (CAN-IO)
// =============================================================================
#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"

typedef struct {
    float    duty_pct;     // 0-100 %
    int      power_limit_w;// watts, from PARAM_compressor_plim
    bool     enable;
    bool     reset;
} CompCmd_t;

typedef struct {
    int      rpm;
    float    duty_pct;
    int      temp_c;       // inverter temperature
    float    hv_voltage;
    int      hv_power_w;
    uint8_t  fault_flags;  // byte 5 of 0x227
    uint8_t  state;
    bool     ready;
    bool     comms_ok;
    uint32_t last_rx_ms;
} CompStatus_t;

void               CanComp_Init(FDCAN_HandleTypeDef *hfdcan);
void               CanComp_Update(void);          // call at 100 ms
void               CanComp_SetDuty(int duty_pct); // 0-100
int                CanComp_GetDuty(void);
void               CanComp_EmergencyStop(void);
const CompStatus_t* CanComp_GetStatus(void);

// FDCAN Rx FIFO0 callback — call from HAL_FDCAN_RxFifo0Callback
void CanComp_RxCallback(FDCAN_HandleTypeDef *hfdcan);
