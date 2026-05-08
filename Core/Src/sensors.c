// sensors.c — Sensor acquisition and conversion
// Ported from Superman-Firmware sensors.cpp (Wim Boone, GPL-3.0)
//
// ADC layout (HOPE G474VET6 verified headers):
//   ADC1 DMA → g_adc1Dma[5]:  PA0, PA1, PA2, PA3, PB0
//   ADC2 DMA → g_adc2Dma[8]:  PA4, PA5, PC0-PC5
//
// PT sensor wiring:
//   Each PT sensor (Suction, Discharge, Subcool) has:
//     Vref → 5V supply from controller
//     Pres → 10kΩ → ADC pin → 10kΩ → GND  (divider ratio 0.5)
//     Temp → 10kΩ → ADC pin → 10kΩ → GND  (divider ratio 0.5)
//     GND  → GND
//   Output range: 0.5V–4.5V ratiometric to 5V Vref
//   After divider: 0.25V–2.25V at ADC pin
//
// Coolant NTC sensors (battery inlet, powertrain inlet):
//   2-wire: Sensor in → 10kΩ pull-up to 3.3V → ADC pin, GND
//   Uses Tesla 10K NTC lookup table (TempMeas_Lookup)
//
// ⚠ PT sensor temperature range constants in config.h need verification
//   against the actual Tesla sensor datasheet during bench testing.

#include "main.h"
#include "sensors.h"
#include "params.h"
#include "config.h"
#include "temp_meas.h"
#include <math.h>

// ─── DMA buffers (placed in DMA_BUFFER section for reliable DMA access) ───────
uint16_t g_adc1Dma[ADC1_BUF_COUNT] __attribute__((section(".DMA_BUFFER")));
uint16_t g_adc2Dma[ADC2_BUF_COUNT] __attribute__((section(".DMA_BUFFER")));

// ─── EWMA filter (α = 0.5, matches Superman-Firmware Ewma class) ──────────────
typedef struct { float value; bool init; } Ewma_t;

static float ewma(Ewma_t *f, float newVal) {
    if (!f->init) { f->value = newVal; f->init = true; }
    f->value = 0.5f * newVal + 0.5f * f->value;
    return f->value;
}

// One filter per sensor channel
static Ewma_t f_pt_disc_pres, f_pt_sc_pres, f_pt_suct_pres;
static Ewma_t f_pt_disc_temp, f_pt_sc_temp, f_pt_suct_temp;
static Ewma_t f_coolant_batt, f_coolant_pt;
static Ewma_t f_coolant_level;

// ─── Init ─────────────────────────────────────────────────────────────────────
void Sensors_Init(ADC_HandleTypeDef *hadc1, ADC_HandleTypeDef *hadc2) {
    if (hadc1) {
        HAL_ADCEx_Calibration_Start(hadc1, ADC_SINGLE_ENDED);
        HAL_ADC_Start_DMA(hadc1, (uint32_t*)g_adc1Dma, ADC1_BUF_COUNT);
    }
    if (hadc2) {
        HAL_ADCEx_Calibration_Start(hadc2, ADC_SINGLE_ENDED);
        HAL_ADC_Start_DMA(hadc2, (uint32_t*)g_adc2Dma, ADC2_BUF_COUNT);
    }
}

// ─── PT sensor: raw ADC → sensor voltage (undoes 10k+10k divider) ─────────────
static inline float _rawToSensorVoltage(uint16_t raw) {
    float v_adc = (float)raw / (float)ADC_MAX_RAW * 3.3f;
    return v_adc / PT_DIVIDER_RATIO;   // × 2.0 for 10k+10k
}

// ─── PT sensor: sensor voltage → pressure (bar) ───────────────────────────────
// Ratiometric output: 0.5V = 0 bar, 4.5V = full_scale_bar
static float _ptPressureBar(uint16_t raw, float full_scale_bar) {
    float v = _rawToSensorVoltage(raw);
    if (v < PT_V_MIN) return 0.0f;
    float p = (v - PT_V_MIN) / PT_V_SPAN * full_scale_bar;
    if (p < 0.0f)          p = 0.0f;
    if (p > full_scale_bar) p = full_scale_bar;
    return p;
}

// ─── PT sensor: sensor voltage → temperature (°C) ────────────────────────────
// Linear voltage output: 0.5V = temp_min, 4.5V = temp_min + temp_span
// ⚠ Verify temp_min and temp_span values during bench testing with known temps
static float _ptTempC(uint16_t raw, float temp_min, float temp_span) {
    float v = _rawToSensorVoltage(raw);
    if (v < PT_V_MIN) v = PT_V_MIN;
    if (v > PT_V_MAX) v = PT_V_MAX;
    return temp_min + (v - PT_V_MIN) / PT_V_SPAN * temp_span;
}

// ─── Coolant level: raw → 0.0 (empty) to 1.0 (full) ──────────────────────────
// Assumes linear capacitive sensor output 0–3.3V
// ⚠ Verify scaling against actual sensor output during testing
static float _coolantLevel(uint16_t raw) {
    float v = (float)raw / (float)ADC_MAX_RAW * 3.3f;
    float level = v / COOLANT_LEVEL_FULL_V;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    return level;
}

// ─── Update — called at 100 ms from Ms100Task ─────────────────────────────────
void Sensors_Update(void) {

    // ── ADC1 channels ─────────────────────────────────────────────────────────

    // [0] PA0 → PT_discharge pressure (high side, 0–37.265 bar)
    Params_SetFloat(PARAM_pressure_outlet_compressor,
        ewma(&f_pt_disc_pres,
             _ptPressureBar(g_adc1Dma[ADC1_IDX_PT_DISC_PRES], PT_DISC_BAR_MAX)));

    // [1] PA1 → PT_SC pressure (subcool/liquid line, 0–10.859 bar)
    Params_SetFloat(PARAM_pressure_pre_evaporator,
        ewma(&f_pt_sc_pres,
             _ptPressureBar(g_adc1Dma[ADC1_IDX_PT_SC_PRES], PT_SC_BAR_MAX)));

    // [2] PA2 → coolant level (capacitive, 0.0–1.0)
    float level = ewma(&f_coolant_level,
                       _coolantLevel(g_adc1Dma[ADC1_IDX_COOLANT_LEVEL]));
    Params_SetFloat(PARAM_reservoir_level, level);

    // [3] PA3 → reserved (future cabin temp) — no conversion

    // [4] PB0 → coolant temp battery inlet (NTC 10K, direct TempMeas_Lookup)
    Params_SetFloat(PARAM_temp_inlet_battery,
        ewma(&f_coolant_batt,
             TempMeas_Lookup(g_adc1Dma[ADC1_IDX_COOLANT_BATT], TEMP_TESLA_10K)));

    // ── ADC2 channels ─────────────────────────────────────────────────────────

    // [0] PA4 → reserved (future cabin temp) — no conversion
    // [1] PA5 → reserved (future cabin temp) — no conversion

    // [2] PC0 → PT_discharge temp (high side)
    // ⚠ PT_DISC_TEMP_MIN / PT_DISC_TEMP_SPAN: verify against sensor datasheet
    Params_SetFloat(PARAM_temp_outlet_compressor,
        ewma(&f_pt_disc_temp,
             _ptTempC(g_adc2Dma[ADC2_IDX_PT_DISC_TEMP],
                      PT_DISC_TEMP_MIN, PT_DISC_TEMP_SPAN)));

    // [3] PC1 → PT_suction temp (low side)
    Params_SetFloat(PARAM_temp_inlet_compressor,
        ewma(&f_pt_suct_temp,
             _ptTempC(g_adc2Dma[ADC2_IDX_PT_SUCT_TEMP],
                      PT_SUCT_TEMP_MIN, PT_SUCT_TEMP_SPAN)));

    // [4] PC2 → PT_SC temp (subcool/liquid line)
    Params_SetFloat(PARAM_temp_pre_evaporator,
        ewma(&f_pt_sc_temp,
             _ptTempC(g_adc2Dma[ADC2_IDX_PT_SC_TEMP],
                      PT_SC_TEMP_MIN, PT_SC_TEMP_SPAN)));

    // [5] PC3 → PT_suction pressure (low side, 0–10.859 bar)
    Params_SetFloat(PARAM_pressure_inlet_compressor,
        ewma(&f_pt_suct_pres,
             _ptPressureBar(g_adc2Dma[ADC2_IDX_PT_SUCT_PRES], PT_SUCT_BAR_MAX)));

    // [6] PC4 → uaux (12V supply monitor) — read via Sensors_GetUaux()

    // [7] PC5 → coolant temp powertrain inlet (NTC 10K)
    Params_SetFloat(PARAM_temp_inlet_powertrain,
        ewma(&f_coolant_pt,
             TempMeas_Lookup(g_adc2Dma[ADC2_IDX_COOLANT_PT], TEMP_TESLA_10K)));
}

// ─── 12V supply voltage ───────────────────────────────────────────────────────
float Sensors_GetUaux(void) {
    return (float)g_adc2Dma[ADC2_IDX_UAUX] / UAUX_GAIN;
}
