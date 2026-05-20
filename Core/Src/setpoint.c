#include "setpoint.h"
#include "params.h"
#include "fan.h"

// Until cabin duct sensors are wired, use setpoint thresholds to request HVAC mode.
#define CABIN_HEAT_THRESHOLD_C  20
#define CABIN_COOL_THRESHOLD_C  16

void Setpoint_Apply(void)
{
    if (Params_GetInt(PARAM_opmode) != OPMODE_DASHBOARD) {
        return;
    }

    int batt_sp = Params_GetInt(PARAM_temp_battery_setp);
    Params_SetInt(PARAM_temp_battery_min, batt_sp - 3);
    Params_SetInt(PARAM_temp_battery_max, batt_sp + 3);

    int csl = Params_GetInt(PARAM_cabin_setp_l);
    int csr = Params_GetInt(PARAM_cabin_setp_r);

    Param_SetInt(heat_cabinl, csl >= CABIN_HEAT_THRESHOLD_C ? 1 : 0);
    Param_SetInt(heat_cabinr, csr >= CABIN_HEAT_THRESHOLD_C ? 1 : 0);
    Param_SetInt(cool_cabin, (csl <= CABIN_COOL_THRESHOLD_C && csr <= CABIN_COOL_THRESHOLD_C) ? 1 : 0);

    uint8_t rfan = (uint8_t)Params_GetInt(PARAM_radiatorfan_pwm);
    if (rfan > 100) {
        rfan = 100;
    }
    Fan_SetRadiatorDuty(rfan);
}
