// can_comp.c — AC Compressor FDCAN driver
// Ported from Superman-Firmware pumps.cpp + interface.cpp (Wim Boone, GPL-3.0)
// Frame formats verified directly from source code.

#include "main.h"
#include "can_comp.h"
#include "params.h"
#include "config.h"
#include <string.h>

static FDCAN_HandleTypeDef *_hfdcan = NULL;
static CompStatus_t _status = {0};
static uint32_t     _lastTxMs = 0;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static inline uint8_t lowByte(int v)  { return (uint8_t)(v & 0xFF); }
static inline uint8_t highByte(int v) { return (uint8_t)((v >> 8) & 0xFF); }

static void _send(uint32_t id, const uint8_t *data) {
    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier          = id;
    hdr.IdType              = FDCAN_STANDARD_ID;
    hdr.TxFrameType         = FDCAN_DATA_FRAME;
    hdr.DataLength          = FDCAN_DLC_BYTES_8;
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker       = 0;
    HAL_FDCAN_AddMessageToTxFifoQ(_hfdcan, &hdr, (uint8_t*)data);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void CanComp_Init(FDCAN_HandleTypeDef *hfdcan) {
    _hfdcan = hfdcan;

    // Accept 0x227, 0x2A8, 0x730, 0x731
    FDCAN_FilterTypeDef f = {0};
    f.IdType       = FDCAN_STANDARD_ID;
    f.FilterType   = FDCAN_FILTER_RANGE;
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterIndex  = 0;
    f.FilterID1    = 0x227;
    f.FilterID2    = 0x731;
    HAL_FDCAN_ConfigFilter(hfdcan, &f);
    HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    HAL_FDCAN_Start(hfdcan);
}

// ─── Update — TX telemetry + watchdog — call at 100 ms ────────────────────────
void CanComp_Update(void) {
    if (!_hfdcan) return;

    // Comms timeout (1 s without status frame)
    if (_status.last_rx_ms > 0 &&
        (HAL_GetTick() - _status.last_rx_ms) > 1000) {
        _status.comms_ok = false;
    }

    // TX at 10 Hz
    if ((HAL_GetTick() - _lastTxMs) < CAN_TX_PERIOD_MS) return;
    _lastTxMs = HAL_GetTick();

    // ── 0x281 Compressor command (from Compressor::SendMessages) ─────────────
    // Bytes 0-1: VCFRONT_CMPTargetDuty in 0.1% increments (little endian)
    // Bytes 2-3: VCFRONT_CMPPowerLimit in watts (little endian)
    // Byte  4:   VCFRONT_CMPReset (0)
    // Byte  5:   VCFRONT_CMPEnable (1 = enable)
    // Bytes 6-7: Reserved (0)
    {
        int duty    = Params_GetInt(PARAM_compressor_duty_request);  // already in 0.1% units
        int plim    = Params_GetInt(PARAM_compressor_plim);
        bool enable = (duty > 0);

        uint8_t d[8] = {0};
        d[0] = lowByte(duty);
        d[1] = highByte(duty);
        d[2] = lowByte(plim);
        d[3] = highByte(plim);
        d[4] = 0x00;
        d[5] = enable ? 0x01 : 0x00;
        d[6] = 0x00;
        d[7] = 0x00;
        _send(CAN_ID_COMP_CMD, d);
    }

    // ── 0x732 HP manifold telemetry 1 (from Interface::SendMessages) ──────────
    // Byte 0: temp_inlet_compressor  + 40 offset
    // Byte 1: temp_outlet_compressor + 40 offset
    // Byte 2: temp_pre_evaporator    + 40 offset
    // Byte 3: pressure_inlet         × 5 (scaled to 0-255)
    // Byte 4: pressure_outlet        × 5
    // Byte 5: pressure_pre_evap      × 5
    // Byte 6: pump_battery_flow      × 10
    // Byte 7: pump_powertrain_flow   × 10
    {
        uint8_t d[8];
        d[0] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_temp_inlet_compressor)  + 40), 8);
        d[1] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_temp_outlet_compressor) + 40), 8);
        d[2] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_temp_pre_evaporator)    + 40), 8);
        d[3] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_pressure_inlet_compressor) * 5), 8);
        d[4] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_pressure_outlet_compressor)* 5), 8);
        d[5] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_pressure_pre_evaporator)   * 5), 8);
        d[6] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_pump_battery_flow)  * 10), 8);
        d[7] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_pump_powertrain_flow)* 10), 8);
        _send(CAN_ID_HP_TELEMETRY_1, d);
    }

    // ── 0x733 HP manifold telemetry 2 ─────────────────────────────────────────
    // Byte 0: temp_inlet_battery    + 40
    // Byte 1: temp_inlet_powertrain + 40
    // Byte 2: reservoir_level
    // Byte 3: 40 (powertrain outlet placeholder)
    // Byte 4: radiatorfan_pwm       + 40
    // Byte 5: reserved
    // Byte 6: octovalve_position
    // Byte 7: compressor speed scaled (change(speed, 800,8000, 20,250))
    {
        int speed = Params_GetInt(PARAM_compressor_speed);
        // Linear map: speed 800→8000 maps to 20→250
        int speed_scaled = 20 + (speed - 800) * (250 - 20) / (8000 - 800);
        if (speed_scaled < 0)   speed_scaled = 0;
        if (speed_scaled > 255) speed_scaled = 255;

        uint8_t d[8];
        d[0] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_temp_inlet_battery)    + 40), 8);
        d[1] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_temp_inlet_powertrain) + 40), 8);
        d[2] = (uint8_t)(Params_GetInt(PARAM_reservoir_level) & 0xFF);
        d[3] = 40;
        d[4] = (uint8_t)__SSAT((int)(Params_GetFloat(PARAM_radiatorfan_pwm) + 40), 8);
        d[5] = 0;
        d[6] = (uint8_t)(Params_GetInt(PARAM_octovalve_position) & 0xFF);
        d[7] = (uint8_t)speed_scaled;
        _send(CAN_ID_HP_TELEMETRY_2, d);
    }
}

// ─── Duty helpers ─────────────────────────────────────────────────────────────
void CanComp_SetDuty(int duty_pct) {
    if (duty_pct < 0)   duty_pct = 0;
    if (duty_pct > 100) duty_pct = 100;
    // CAN message value is in 0.1% increments (×10)
    Params_SetInt(PARAM_compressor_duty_request, duty_pct * 10);
}

int CanComp_GetDuty(void) {
    return Params_GetInt(PARAM_compressor_duty);  // actual duty from compressor
}

void CanComp_EmergencyStop(void) {
    CanComp_SetDuty(0);
    _lastTxMs = 0;  // force immediate TX
    CanComp_Update();
}

const CompStatus_t* CanComp_GetStatus(void) { return &_status; }

// ─── FDCAN Rx FIFO0 callback ──────────────────────────────────────────────────
void CanComp_RxCallback(FDCAN_HandleTypeDef *hfdcan) {
    if (hfdcan != _hfdcan) return;
    FDCAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, data) == HAL_OK) {

        switch (hdr.Identifier) {

        // ── 0x227 Model 3 compressor status (from Compressor::handle227) ─────
        // Byte 0-1: CMP_speedRPM (little endian)
        // Byte 2-3: CMP_speedDuty (0.1% LE)
        // Byte 4:   CMP_inverterTemperature (°C, offset -40)
        // Byte 5:   Fault flags
        //           bit0=HVOverVolt bit1=HVUnderVolt bit2=OverTemp bit3=UnderTemp
        //           bit4=CANTimeout bit5=OverCurrent bit6=MotorVoltageSat bit7=CurrentSensorCal
        // Byte 6:   upper nibble = CMP_state, lower nibble = additional faults
        //           bit0=failedStart bit1=shortCircuit bit2=repeatOverCurrent
        // Byte 7:   bit7 = CMP_ready
        case CAN_ID_COMP_STATUS_227:
            _status.rpm       = (int)((uint16_t)data[1] << 8 | data[0]);
            _status.duty_pct  = (float)((uint16_t)data[3] << 8 | data[2]) * 0.1f;
            _status.temp_c    = (int)data[4] - 40;
            _status.fault_flags = data[5];
            _status.state     = (data[6] >> 4) & 0x0F;
            _status.ready     = (data[7] & 0x80) != 0;
            _status.comms_ok  = true;
            _status.last_rx_ms = HAL_GetTick();

            Params_SetInt(PARAM_compressor_speed, _status.rpm);
            Params_SetFloat(PARAM_compressor_duty, _status.duty_pct);
            Params_SetInt(PARAM_compressor_temp,   _status.temp_c);
            break;

        // ── 0x2A8 Compressor HV status (from Compressor::handle2A8) ──────────
        // Bits 0-10:  RPM (scale 10 RPM/bit)
        // Bits 11-20: Speed duty (scale 0.1%/bit)
        // Bits 21-31: Input HV power (scale 10W/bit)
        // Bits 32-40: Input HV current (scale 0.1A/bit)
        // Bits 41-51: Input HV voltage (scale 0.5V/bit)
        // Bits 55-63: State flags
        case CAN_ID_COMP_STATUS_2A8: {
            uint16_t rpm_raw   = (uint16_t)(data[0] | ((data[1] & 0x07) << 8));
            uint16_t power_raw = (uint16_t)((data[2] >> 6) | (data[3] << 2) | ((data[4] & 0x01) << 10));
            uint16_t volt_raw  = (uint16_t)((data[5] >> 1) | (data[6] << 7)) & 0x7FF;

            _status.hv_power_w  = (int)(rpm_raw) * 10;  // rpm_raw reused for power here per original
            _status.hv_voltage  = (float)volt_raw * 0.5f;

            Params_SetInt(PARAM_compressor_power, power_raw * 10);
            break;
        }

        // ── 0x731 CAN-IO setpoints (from Interface::handle731) ───────────────
        // Only active when canio == CAN_IO_MODE
        // Byte 0: battery temp    (-40 offset)
        // Byte 1: battery setpoint
        // Byte 3: evaporator setpoint
        // Byte 5: condenser setpoint
        // CAN_ID_SETPOINTS (0x731) — future use
        // When a vehicle CAN network is available, battery and powertrain
        // temperatures can be received here instead of from local NTC sensors.
        // Uncomment and add CAN_ID_SETPOINTS back to config.h to enable.
        //
        // case CAN_ID_SETPOINTS:
        //     if (Params_GetInt(PARAM_canio) == CAN_IO_MODE) {
        //         Params_SetFloat(PARAM_temp_battery,         (float)data[0] - 40.0f);
        //         Params_SetInt  (PARAM_temp_battery_setp,    (int)data[1]);
        //         Params_SetInt  (PARAM_temp_evaporator_setp, (int)data[3]);
        //         Params_SetInt  (PARAM_temp_condensor_setp,  (int)data[5]);
        //     }
        //     break;

        default:
            break;
        }
    }
}
