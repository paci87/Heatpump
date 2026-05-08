// thermal_control.c — Full thermal management logic
// Direct port of Superman-Firmware thermal_control.cpp (Wim Boone, GPL-3.0)
// Replaces: Param:: → Param_Get/SetInt/Float macros
//           Compressor:: → CanComp_ functions
//           Valve:: → Valve_ functions
//           Waterpump:: → Pumps_ functions

#include "main.h"
#include "thermal_control.h"
#include "params.h"
#include "config.h"
#include "valves.h"
#include "can_comp.h"
#include "pumps.h"
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>

// ─── PI controller state ──────────────────────────────────────────────────────
static float s_accumulated_error = 0.0f;
static float s_last_error        = 0.0f;
static const float Kp = 1.0f;
static const float Ki = 0.1f;
static const float dt = 0.1f;  // 100 ms loop time

// ─── Source and sink types ────────────────────────────────────────────────────
typedef enum { SOURCE_AMBIENT = 0, SOURCE_BATTERY = 1, SOURCE_RECIRCULATION = 2 } SourceType_t;
typedef enum { SINK_AMBIENT   = 0, SINK_BATTERY   = 1 }                           SinkType_t;
typedef enum { PASSIVE = 0, DOMINANT_HEATING = 1, DOMINANT_COOLING = 2 }          HeatTransferMode_t;

typedef struct { bool available; float temp; } ThermalComponent_t;
typedef struct { SourceType_t type; ThermalComponent_t info; } SourceData_t;
typedef struct { SinkType_t   type; ThermalComponent_t info; } SinkData_t;

typedef struct {
    bool cabinLHeating;
    bool cabinRHeating;
    bool cabinCooling;
    bool batteryHeating;
    bool batteryCooling;
    bool powertrainCooling;
    bool radiatorDefrost;
} ThermalDemands_t;

// ─── Clamp helpers ────────────────────────────────────────────────────────────
static inline float _clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
static inline int _clampi(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
// Linear map: value x in [x0,x1] → [y0,y1]
static inline int _linearMap(int x, int x0, int x1, int y0, int y1) {
    if (x <= x0) return y0;
    if (x >= x1) return y1;
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}

// ─── Assess thermal demands ────────────────────────────────────────────────────
static ThermalDemands_t _assessDemands(void) {
    ThermalDemands_t d = {0};

    // Cabin demands from digital inputs (set by Ms10Task reading GPIO)
    d.cabinLHeating    = Param_GetBool(heat_cabinl);
    d.cabinRHeating    = Param_GetBool(heat_cabinr);
    d.cabinCooling     = Param_GetBool(cool_cabin);

    // Battery and powertrain: automatic from temperature vs. thresholds
    int battTemp       = Param_GetInt(temp_battery);
    d.batteryHeating   = battTemp < Param_GetInt(temp_battery_min);
    d.batteryCooling   = battTemp > Param_GetInt(temp_battery_max);

    int ptTemp         = Param_GetInt(temp_powertrain);
    d.powertrainCooling = ptTemp > Param_GetInt(temp_powertrain_max);

    d.radiatorDefrost  = false;  // TODO: implement

    // Bitmask for display param
    int bits = 0;
    if (d.cabinCooling)      bits |= (1 << 0);
    if (d.batteryCooling)    bits |= (1 << 1);
    if (d.powertrainCooling) bits |= (1 << 2);
    if (d.cabinLHeating)     bits |= (1 << 3);
    if (d.cabinRHeating)     bits |= (1 << 4);
    if (d.batteryHeating)    bits |= (1 << 5);
    if (d.radiatorDefrost)   bits |= (1 << 6);
    Param_SetInt(thermal_demands, bits);

    return d;
}

// ─── Source/sink availability ─────────────────────────────────────────────────
static void _getSourcesSinks(const ThermalDemands_t *d,
                              SourceData_t src[3], SinkData_t snk[2]) {
    float ambTemp  = Param_GetFloat(temp_ambient);
    float battTemp = Param_GetFloat(temp_battery);
    float recircT  = Param_GetFloat(temp_outlet_compressor);

    src[0] = (SourceData_t){ SOURCE_BATTERY,       { !d->batteryHeating, battTemp } };
    src[1] = (SourceData_t){ SOURCE_AMBIENT,        { true,               ambTemp  } };
    src[2] = (SourceData_t){ SOURCE_RECIRCULATION,  { true,               recircT  } };

    snk[0] = (SinkData_t)  { SINK_BATTERY,          { !d->batteryCooling, battTemp } };
    snk[1] = (SinkData_t)  { SINK_AMBIENT,           { true,              ambTemp  } };
}

// ─── Select hottest source (smallest delta-T to target → best COP) ────────────
static SourceType_t _selectBestSource(float targetTemp, const SourceData_t src[3]) {
    SourceType_t best = SOURCE_AMBIENT;
    float minDelta = 3.4e38f;

    for (int i = 0; i < 3; i++) {
        if (src[i].info.available) {
            float delta = targetTemp - src[i].info.temp;
            if (delta < minDelta) { minDelta = delta; best = src[i].type; }
        }
    }
    if (Param_GetInt(temp_ambient) < (int)RECIRC_TEMP_THRESHOLD)
        best = SOURCE_RECIRCULATION;

    Param_SetInt(best_source, (int)best);
    return best;
}

// ─── Select coldest sink (largest delta-T → best heat rejection) ──────────────
static SinkType_t _selectBestSink(float targetTemp, const SinkData_t snk[2]) {
    SinkType_t best = SINK_AMBIENT;
    float maxDelta  = -3.4e38f;

    for (int i = 0; i < 2; i++) {
        if (snk[i].info.available) {
            float delta = targetTemp - snk[i].info.temp;
            if (delta > maxDelta) { maxDelta = delta; best = snk[i].type; }
        }
    }
    Param_SetInt(best_sink, (int)best);
    return best;
}

// ─── PI controller for compressor duty ────────────────────────────────────────
static uint8_t _runPiControl(float setpoint, float measured) {
    float highPressure = Param_GetFloat(pressure_outlet_compressor);
    float lowPressure  = Param_GetFloat(pressure_pre_evaporator);
    int   currentDuty  = CanComp_GetDuty();

    // Pressure safety overrides
    if (highPressure > HIGH_PRESSURE_LIMIT) {
        int nd = currentDuty - 1;
        if (nd < 0) nd = 0;
        CanComp_SetDuty(nd);
        return (uint8_t)(nd * 2.55f);
    }
    if (lowPressure < LOW_PRESSURE_LIMIT) {
        int nd = currentDuty - 1;
        if (nd < 0) nd = 0;
        CanComp_SetDuty(nd);
        return (uint8_t)(nd * 2.55f);
    }

    // PI with anti-windup
    float error = setpoint - measured;
    s_accumulated_error += error * dt;
    if (error * s_last_error < 0.0f)
        s_accumulated_error *= 0.5f;  // decay on sign reversal
    s_last_error = error;

    float out = Kp * error + Ki * s_accumulated_error;
    out = _clampf(out, 0.0f, 100.0f);

    // Anti-windup clamping
    if (out >= 100.0f)
        s_accumulated_error = _clampf(s_accumulated_error,
                                      -3.4e38f, (100.0f - Kp * error) / Ki);
    if (out <= 0.0f)
        s_accumulated_error = _clampf(s_accumulated_error,
                                      (-Kp * error) / Ki, 3.4e38f);

    CanComp_SetDuty((int)out);
    uint8_t valve_out = (uint8_t)(out * 2.55f);
    if (valve_out < (uint8_t)MIN_VALVE_POSITION) valve_out = (uint8_t)MIN_VALVE_POSITION;
    return valve_out;
}

// ─── Condenser split adjustment (heating mode) ────────────────────────────────
// Reduce coolant condenser opening when compressor is struggling to meet setpoint
static void _adjustCondenserSplit(uint8_t *cabinL, uint8_t *cabinR,
                                   uint8_t *coolant, uint8_t compDuty) {
    float condSetpt = (float)Param_GetInt(temp_condensor_setp);
    float condActual= Param_GetFloat(temp_outlet_compressor);

    if (compDuty > 90 && (condSetpt - condActual) > 2.0f) {
        if (*coolant > 128) *coolant -= 5;
    } else if (compDuty < 80 &&
               (Param_GetInt(temp_battery_min) - Param_GetInt(temp_battery)) > 2) {
        if (*coolant < 255) *coolant += 5;
    }
    (void)cabinL; (void)cabinR;
}

// ─── Evaporator split adjustment (cooling mode) ───────────────────────────────
static void _adjustEvaporatorSplit(uint8_t *cabin, uint8_t *coolant, uint8_t compDuty) {
    float evapSetpt = (float)Param_GetInt(temp_evaporator_setp);
    float evapActual= Param_GetFloat(temp_inlet_compressor);

    if (compDuty > 90 && (evapActual - evapSetpt) > 2.0f) {
        if (*coolant > 0) *coolant -= 5;
    } else if (compDuty < 80 &&
               (Param_GetInt(temp_battery) - Param_GetInt(temp_battery_max)) > 2) {
        if (*coolant < 255) *coolant += 5;
    }
    (void)cabin;
}

// ─── Top-level thermal control — call every 100 ms ────────────────────────────
void thermalControl(void) {
    ThermalDemands_t demands = _assessDemands();
    SourceData_t sources[3];
    SinkData_t   sinks[2];
    _getSourcesSinks(&demands, sources, sinks);

    // Dynamic condenser setpoint: 50°C at ambient ≤ -20°C, 70°C at ambient ≥ 10°C
    int ambient = Param_GetInt(temp_ambient);
    int condSetpt = _clampi(
        _linearMap(ambient, -20, 10, CONDENSOR_SETP_MIN, CONDENSOR_SETP_MAX),
        CONDENSOR_SETP_MIN, CONDENSOR_SETP_MAX);

    // Dynamic evaporator setpoint: 5°C at ambient ≤ 30°C, 10°C at ambient ≥ 50°C
    int evapSetpt = _clampi(
        _linearMap(ambient, 30, 50, EVAP_SETP_MIN, EVAP_SETP_MAX),
        EVAP_SETP_MIN, EVAP_SETP_MAX);

    Param_SetInt(temp_condensor_setp,  condSetpt);
    Param_SetInt(temp_evaporator_setp, evapSetpt);

    SourceType_t bestSource = _selectBestSource((float)condSetpt, sources);
    SinkType_t   bestSink   = _selectBestSink  ((float)evapSetpt, sinks);

    // Tie pump duty to compressor (30-80% range, from thermal_control.cpp)
    int pumpDuty = Param_GetInt(compressor_duty_request) / 10; // convert from 0.1% back to %
    if (pumpDuty < 30) pumpDuty = 30;
    if (pumpDuty > 80) pumpDuty = 80;
    Pumps_PowertrainSetDuty((uint8_t)pumpDuty);
    Pumps_BatterySetDuty   ((uint8_t)pumpDuty);

    // ── Octovalve routing selection ───────────────────────────────────────────
    // OctoPos: POS2_SERIES    Condensor→Radiator→Evaporator→Battery→Powertrain
    //          POS3_AMBIENT   {Condensor→Battery→PT} + {Evaporator→Radiator}
    //          POS4_RBYPASS   Condensor→Evaporator→Battery→Powertrain
    //          POS5_PARALLEL  {Condensor→Radiator→PT} + {Evaporator→Battery}
    if (   (demands.batteryCooling && demands.powertrainCooling)
        || (demands.cabinCooling   && bestSink   == SINK_AMBIENT)
        || ((demands.cabinLHeating || demands.cabinRHeating) && bestSource == SOURCE_BATTERY))
        Valve_OctoSetPos(OCTO_POS2_SERIES);

    else if (((demands.cabinLHeating || demands.cabinRHeating || demands.batteryHeating)
               && bestSource == SOURCE_AMBIENT)
          || (demands.cabinCooling && bestSink == SINK_BATTERY))
        Valve_OctoSetPos(OCTO_POS3_AMBIENT);

    else if ((demands.cabinLHeating || demands.cabinRHeating || demands.batteryHeating)
              && bestSource == SOURCE_RECIRCULATION)
        Valve_OctoSetPos(OCTO_POS4_RBYPASS);

    else if ((demands.cabinCooling || demands.batteryCooling)
              && !demands.batteryHeating && bestSink == SINK_AMBIENT)
        Valve_OctoSetPos(OCTO_POS5_PARALLEL);

    // ── EXV control ───────────────────────────────────────────────────────────
    if (demands.cabinLHeating || demands.cabinRHeating
        || (demands.batteryHeating && !demands.cabinCooling))
    {
        // ── Dominant heating mode ─────────────────────────────────────────────
        Param_SetInt(heat_transfer_mode, DOMINANT_HEATING);

        float setpt   = (float)Param_GetInt(temp_condensor_setp);
        float measd   = Param_GetFloat(temp_outlet_compressor);
        uint8_t ctrlOut = _runPiControl(setpt, measd);
        uint8_t compDuty = (uint8_t)CanComp_GetDuty();

        uint8_t cabinL = demands.cabinLHeating  ? 255 : 0;
        uint8_t cabinR = demands.cabinRHeating  ? 255 : 0;
        uint8_t cool   = demands.batteryHeating ? 255 : 0;

        _adjustCondenserSplit(&cabinL, &cabinR, &cool, compDuty);

        // Condenser valves distribute refrigerant to heated loops
        Valve_CoolantCondensorClose();
        Valve_ExpansionSetPos(EXPV_CONDENSOR_COOLANT, cool);
        Valve_ExpansionSetPos(EXPV_CONDENSOR_CABINR,  cabinR);
        Valve_ExpansionSetPos(EXPV_CONDENSOR_CABINL,  cabinL);

        // Evaporator absorbs heat from source
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_COOLANT,
            (bestSource != SOURCE_RECIRCULATION) ? ctrlOut : 0);
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_CABIN,  0);  // cabin never a source
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_RECIRC,
            (bestSource == SOURCE_RECIRCULATION) ? ctrlOut : 0);
    }
    else if (demands.cabinCooling || demands.batteryCooling || demands.powertrainCooling)
    {
        // ── Dominant cooling mode ─────────────────────────────────────────────
        Param_SetInt(heat_transfer_mode, DOMINANT_COOLING);

        float setpt   = (float)Param_GetInt(temp_evaporator_setp);
        float measd   = Param_GetFloat(temp_inlet_compressor);
        uint8_t ctrlOut = _runPiControl(setpt, measd);
        uint8_t compDuty = (uint8_t)CanComp_GetDuty();

        uint8_t cabin  = demands.cabinCooling  ? ctrlOut : 0;
        uint8_t cool   = (demands.batteryCooling || demands.powertrainCooling) ? ctrlOut : 0;

        _adjustEvaporatorSplit(&cabin, &cool, compDuty);

        // Condenser rejects heat through coolant loop
        Valve_CoolantCondensorOpen();
        Valve_ExpansionSetPos(EXPV_CONDENSOR_COOLANT, 0);
        Valve_ExpansionSetPos(EXPV_CONDENSOR_CABINR,  0);
        Valve_ExpansionSetPos(EXPV_CONDENSOR_CABINL,  0);

        // Evaporator absorbs heat from load
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_COOLANT, cool);
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_CABIN,   cabin);
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_RECIRC,  0);  // self-cooling impossible
    }
    else
    {
        // ── Idle / passive ────────────────────────────────────────────────────
        Param_SetInt(heat_transfer_mode, PASSIVE);

        // Leave all valves open when idle (don't block compressor on next start)
        Valve_CoolantCondensorOpen();
        Valve_ExpansionSetPos(EXPV_CONDENSOR_COOLANT,  255);
        Valve_ExpansionSetPos(EXPV_CONDENSOR_CABINR,   255);
        Valve_ExpansionSetPos(EXPV_CONDENSOR_CABINL,   255);
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_COOLANT, 255);
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_CABIN,   255);
        Valve_ExpansionSetPos(EXPV_EVAPORATOR_RECIRC,  255);

        CanComp_SetDuty(0);  // compressor off when idle
    }
}
