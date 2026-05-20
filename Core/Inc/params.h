#pragma once
// =============================================================================
//  params.h — Parameter store (replaces libopeninv Param system)
//  Matches all Param:: names used in Superman-Firmware
// =============================================================================
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// ─── Parameter IDs (matching param_prj.h, used for flash storage) ────────────
typedef enum {
    // Saveable (non-zero ID)
    PARAM_canspeed            = 0,
    PARAM_nodeid              = 1,
    PARAM_canio               = 2,
    PARAM_temp_battery_setp   = 3,
    PARAM_temp_battery_min    = 4,
    PARAM_temp_battery_max    = 5,
    PARAM_temp_powertrain_min = 6,
    PARAM_temp_powertrain_max = 7,
    PARAM_temp_evaporator_setp= 8,
    PARAM_temp_condensor_setp = 9,
    PARAM_compressor_plim     = 10,
    // Display values (ID > 2000 by convention)
    PARAM_octovalve_position  = 2005,
    PARAM_octovalve_setpoint  = 2044,
    PARAM_heat_transfer_mode  = 2046,
    PARAM_best_sink           = 2047,
    PARAM_best_source         = 2048,
    PARAM_thermal_demands     = 2045,
    PARAM_expv_recirculation  = 2007,
    PARAM_expv_condensor_coolant= 2008,
    PARAM_expv_condensor_cabinr = 2009,
    PARAM_expv_condensor_cabinl = 2010,
    PARAM_expv_evaporator_coolant=2011,
    PARAM_expv_chiller = 2012,
    PARAM_valve_coolant_condensor=2013,
    PARAM_pressure_inlet_compressor=2014,
    PARAM_pressure_outlet_compressor=2015,
    PARAM_pressure_pre_evaporator=2016,
    PARAM_temp_inlet_compressor=2017,
    PARAM_temp_outlet_compressor=2018,
    PARAM_temp_pre_evaporator  =2019,
    PARAM_temp_inlet_battery   =2020,
    PARAM_temp_inlet_powertrain=2021,
    PARAM_temp_ambient         =2022,
    PARAM_temp_radiator        =2023,
    PARAM_temp_battery         =2024,
    PARAM_temp_powertrain      =2025,
    PARAM_reservoir_level      =2026,
    PARAM_cool_cabin           =2029,
    PARAM_heat_cabinl          =2030,
    PARAM_heat_cabinr          =2031,
    PARAM_preheat_req          =2032,
    PARAM_pump_battery_flow    =2033,
    PARAM_pump_battery_duty    =2034,
    PARAM_pump_powertrain_flow =2035,
    PARAM_pump_powertrain_duty =2036,
    PARAM_compressor_enable    =2037,
    PARAM_compressor_speed     =2038,
    PARAM_compressor_duty_request=2039,
    PARAM_compressor_duty      =2040,
    PARAM_compressor_temp      =2041,
    PARAM_compressor_power     =2042,
    PARAM_radiatorfan_pwm      =2043,
    PARAM_opmode               =2000,
    PARAM_uaux                 =2003,
    PARAM_cpuload              =2004,
    PARAM_cabin_setp_l         =2050,
    PARAM_cabin_setp_r         =2051,
    PARAM_cabin_fan_pwm        =2052,
    PARAM_COUNT,
} ParamId_t;

// CAN-IO mode
typedef enum { HW_IO = 0, CAN_IO_MODE = 1 } CanIoMode_t;

// ─── Parameter value store ────────────────────────────────────────────────────
void  Params_Init(void);
void  Params_Load(void);          // load from Flash
void  Params_Save(void);          // save to Flash

void  Params_SetInt(ParamId_t id, int32_t val);
void  Params_SetFloat(ParamId_t id, float val);
int32_t Params_GetInt(ParamId_t id);
float   Params_GetFloat(ParamId_t id);
bool    Params_GetBool(ParamId_t id);

// Convenience shorthand macros (matching Superman-Firmware call style)
#define Param_SetInt(name, val)   Params_SetInt(PARAM_##name, (int32_t)(val))
#define Param_SetFloat(name, val) Params_SetFloat(PARAM_##name, (float)(val))
#define Param_GetInt(name)        Params_GetInt(PARAM_##name)
#define Param_GetFloat(name)      Params_GetFloat(PARAM_##name)
#define Param_GetBool(name)       Params_GetBool(PARAM_##name)
