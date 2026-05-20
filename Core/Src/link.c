// link.c — newline-delimited JSON telemetry + set commands (USART1 @ 115200)
//
// Telemetry (5 Hz): {"t":"tel", ...}
// Set command:       {"t":"set","cbl":22,"cbr":21,"bset":25,"cfan":50,"rfan":30,"mode":1}

#include "link.h"
#include "usart.h"
#include "params.h"
#include "config.h"
#include "setpoint.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RX_BUF_SIZE  192
#define TX_BUF_SIZE  512

static uint8_t  s_rx_buf[RX_BUF_SIZE];
static uint16_t s_rx_len;
static uint32_t s_last_tel_ms;
static char     s_tx[TX_BUF_SIZE];

static int _parse_int_field(const char *json, const char *key, int *out)
{
    char pattern[24];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return -1;
    }
    p += strlen(pattern);
    *out = (int)strtol(p, NULL, 10);
    return 0;
}

static void _handle_set_line(const char *line)
{
    if (!strstr(line, "\"t\":\"set\"") && !strstr(line, "\"t\": \"set\"")) {
        return;
    }

    int v;
    if (_parse_int_field(line, "cbl", &v) == 0) {
        Params_SetInt(PARAM_cabin_setp_l, v);
    }
    if (_parse_int_field(line, "cbr", &v) == 0) {
        Params_SetInt(PARAM_cabin_setp_r, v);
    }
    if (_parse_int_field(line, "bset", &v) == 0) {
        Params_SetInt(PARAM_temp_battery_setp, v);
    }
    if (_parse_int_field(line, "cfan", &v) == 0) {
        if (v < 0) {
            v = 0;
        }
        if (v > 100) {
            v = 100;
        }
        Params_SetInt(PARAM_cabin_fan_pwm, v);
    }
    if (_parse_int_field(line, "rfan", &v) == 0) {
        if (v < 0) {
            v = 0;
        }
        if (v > 100) {
            v = 100;
        }
        Params_SetInt(PARAM_radiatorfan_pwm, v);
    }
    if (_parse_int_field(line, "mode", &v) == 0) {
        Params_SetInt(PARAM_opmode, v ? OPMODE_DASHBOARD : OPMODE_GPIO);
    }
}

static void _send_telemetry(void)
{
    int n = snprintf(s_tx, sizeof(s_tx),
        "{\"t\":\"tel\",\"ms\":%lu,"
        "\"tb\":%d,\"tp\":%d,\"ta\":%d,"
        "\"p_hi\":%d,\"p_lo\":%d,\"p_sc\":%d,"
        "\"t_disc\":%d,\"t_suct\":%d,\"t_sc\":%d,"
        "\"crpm\":%d,\"cdty\":%d,\"ctemp\":%d,\"cpwr\":%d,"
        "\"pb\":%d,\"pp\":%d,\"fb\":%d,\"fp\":%d,"
        "\"oct\":%d,\"oct_sp\":%d,"
        "\"exv\":[%d,%d,%d,%d,%d,%d],"
        "\"sov\":%d,\"dem\":%d,\"htm\":%d,"
        "\"cbl\":%d,\"cbr\":%d,\"bset\":%d,\"cfan\":%d,\"rfan\":%d,\"mode\":%d}\n",
        (unsigned long)HAL_GetTick(),
        (int)(Params_GetFloat(PARAM_temp_battery) * 10.0f),
        (int)(Params_GetFloat(PARAM_temp_powertrain) * 10.0f),
        (int)(Params_GetFloat(PARAM_temp_ambient) * 10.0f),
        (int)(Params_GetFloat(PARAM_pressure_outlet_compressor) * 10.0f),
        (int)(Params_GetFloat(PARAM_pressure_inlet_compressor) * 10.0f),
        (int)(Params_GetFloat(PARAM_pressure_pre_evaporator) * 10.0f),
        (int)(Params_GetFloat(PARAM_temp_outlet_compressor) * 10.0f),
        (int)(Params_GetFloat(PARAM_temp_inlet_compressor) * 10.0f),
        (int)(Params_GetFloat(PARAM_temp_pre_evaporator) * 10.0f),
        Params_GetInt(PARAM_compressor_speed),
        (int)(Params_GetFloat(PARAM_compressor_duty) * 10.0f),
        Params_GetInt(PARAM_compressor_temp),
        Params_GetInt(PARAM_compressor_power),
        Params_GetInt(PARAM_pump_battery_duty),
        Params_GetInt(PARAM_pump_powertrain_duty),
        (int)(Params_GetFloat(PARAM_pump_battery_flow)),
        (int)(Params_GetFloat(PARAM_pump_powertrain_flow)),
        Params_GetInt(PARAM_octovalve_position),
        Params_GetInt(PARAM_octovalve_setpoint),
        Params_GetInt(PARAM_expv_condensor_coolant),
        Params_GetInt(PARAM_expv_condensor_cabinl),
        Params_GetInt(PARAM_expv_recirculation),
        Params_GetInt(PARAM_expv_condensor_cabinr),
        Params_GetInt(PARAM_expv_evaporator_coolant),
        Params_GetInt(PARAM_expv_chiller),
        Params_GetInt(PARAM_valve_coolant_condensor),
        Params_GetInt(PARAM_thermal_demands),
        Params_GetInt(PARAM_heat_transfer_mode),
        Params_GetInt(PARAM_cabin_setp_l),
        Params_GetInt(PARAM_cabin_setp_r),
        Params_GetInt(PARAM_temp_battery_setp),
        Params_GetInt(PARAM_cabin_fan_pwm),
        Params_GetInt(PARAM_radiatorfan_pwm),
        Params_GetInt(PARAM_opmode));

    if (n > 0 && n < (int)sizeof(s_tx)) {
        HAL_UART_Transmit(&huart1, (uint8_t *)s_tx, (uint16_t)n, 50);
    }
}

void Link_Init(void)
{
    MX_USART1_UART_Init();
    s_rx_len     = 0;
    s_last_tel_ms = 0;
}

void Link_Process(void)
{
    uint8_t byte;
    while (HAL_UART_Receive(&huart1, &byte, 1, 0) == HAL_OK) {
        if (byte == '\n' || byte == '\r') {
            if (s_rx_len > 0) {
                s_rx_buf[s_rx_len] = '\0';
                _handle_set_line((char *)s_rx_buf);
                s_rx_len = 0;
            }
        } else if (s_rx_len < RX_BUF_SIZE - 1) {
            s_rx_buf[s_rx_len++] = byte;
        } else {
            s_rx_len = 0;
        }
    }

    uint32_t now = HAL_GetTick();
    if ((now - s_last_tel_ms) >= 200) {
        s_last_tel_ms = now;
        _send_telemetry();
    }
}

int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, &c, 1, 10);
    return ch;
}
