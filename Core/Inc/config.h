#pragma once
// =============================================================================
//  config.h  —  Tesla Model 3/Y Heat Pump Controller (STM32G474VET6)
//  Ported from Superman-Firmware (Wim Boone, GPL-3.0)
//  github.com/Wim426F/Superman-Firmware
//
//  Target: STM32G474VET6 LQFP100
//  Toolchain: STM32CubeMX + HAL + arm-none-eabi-gcc
//  Clock: HSE 8 MHz → PLL → 170 MHz SYSCLK
//  Board: HOPE G474VET6
// =============================================================================
//
//  PIN MAP — verified against HOPE G474VET6 physical headers
//
//  EXV stepper GPIO (direct, toggled by TIM6 1 kHz ISR):
//    EXV1/LCC:     PE2(DIR)  PE3(STEP)  PE4(EN)
//    EXV2/CC_L:    PB3(DIR)  PB4(STEP)  PB5(EN)
//    EXV3/RECIRC:  PA15(DIR) PC10(STEP) PC11(EN)
//    EXV4/CC_R:    PC12(DIR) PD0(STEP)  PD1(EN)
//    EXV5/EVAP:    PD2(DIR)  PD3(STEP)  PD4(EN)
//    EXV6/CHILLER: PD5(DIR)  PD6(STEP)  PD7(EN)
//
//  Octovalve motor (direct GPIO):
//    PC13 = OCTO_IN1, PC14 = OCTO_IN2
//    PE12 = OCTO_PULSE (EXTI12, both edges) ← remapped from PA8 (not on header)
//
//  Pumps (TIM4 CH1/CH2, PB6/PB7):
//    PB6  = PUMP_BATT_PWM (TIM4_CH1, 10 Hz)
//    PB7  = PUMP_PT_PWM   (TIM4_CH2, 10 Hz)
//    PB8  = PUMP_BATT_FB  (TIM4_CH3, input capture)
//    PB9  = PUMP_PT_FB    (TIM4_CH4, input capture)
//
//  Fan (TIM2_CH4, PB11):
//    PB11 = FAN_PWM (100 Hz)
//
//  Evap shut-off valve (GPIO, active HIGH):
//    PE11 = VALVE_EVAP_SOV
//
//  ADC inputs — verified against HOPE G474VET6 headers:
//
//  ADC1 (5 channels, DMA circular):
//    PA0 → PT_discharge_pressure  (high side, 0–37.265 bar)
//    PA1 → PT_SC_pressure         (subcool/liquid line, 0–10.859 bar)
//    PA2 → coolant_level          (capacitive sensor, analog)
//    PA3 → RESERVED               (future cabin temp sensor)
//    PB0 → coolant_temp_batt_in   (NTC 10K, battery coolant inlet)
//
//  ADC2 (8 channels, DMA circular):
//    PA4 → RESERVED               (future cabin temp sensor)
//    PA5 → RESERVED               (future cabin temp sensor)
//    PC0 → PT_discharge_temp      (high side temp, via 10k+10k divider)
//    PC1 → PT_suction_temp        (low side temp, via 10k+10k divider)
//    PC2 → PT_SC_temp             (subcool temp, via 10k+10k divider)
//    PC3 → PT_suction_pressure    (low side, 0–10.859 bar)
//    PC4 → uaux                   (12V supply monitor)
//    PC5 → coolant_temp_pt_in     (NTC 10K, powertrain coolant inlet)
//
//  Digital inputs (PE8, PE9, PE10, PB2):
//    PE8  = cabin_heat_right
//    PE9  = cabin_cool
//    PE10 = cabin_heat_left
//    PB2  = preheat_req
//
//  FDCAN1: PA11(RX) PA12(TX)
//  USART1: PA9(TX) PA10(RX) — debug console header on board
// =============================================================================

#include "stm32g4xx_hal.h"

// ─── EXV definitions ──────────────────────────────────────────────────────────
// Naming matches spec document (Part 2.2)
#define EXV_LCC      1   // Liquid Cooled Condenser
#define EXV_CC_L     2   // Cabin Condenser Left
#define EXV_RECIRC   3   // Recirculation
#define EXV_CC_R     4   // Cabin Condenser Right
#define EXV_EVAP     5   // Evaporator
#define EXV_CHILLER  6   // Chiller
#define EXV_COUNT    6

// Legacy aliases (used in thermal_control.c — matches Superman-Firmware names)
#define EXPV_CONDENSOR_COOLANT   EXV_LCC
#define EXPV_CONDENSOR_CABINL    EXV_CC_L
#define EXPV_EVAPORATOR_RECIRC   EXV_RECIRC
#define EXPV_CONDENSOR_CABINR    EXV_CC_R
#define EXPV_EVAPORATOR_COOLANT  EXV_EVAP
#define EXPV_EVAPORATOR_CABIN    EXV_CHILLER

// EXV position: 0 = closed, 255 = fully open (matches Superman-Firmware)
#define EXV_POS_CLOSED    0
#define EXV_POS_OPEN      255
#define EXV_POS_PARTIAL   80    // ~31% open, used in COP_BLEND mode

// ─── EXV GPIO pin tables ──────────────────────────────────────────────────────
typedef struct { GPIO_TypeDef *port; uint16_t pin; } GpioPin_t;

//  EXV1/LCC remapped: PE2/PE3/PE4 not on HOPE G474VET6 headers
//  Remapped to: STEP=PE13, DIR=PE14, EN=PE15
//  ⚠ Update CubeMX: add PE13/PE14/PE15 as GPIO_Output
#define EXV_STEP_PINS { \
    {GPIOE, GPIO_PIN_13}, {GPIOB, GPIO_PIN_4},  {GPIOC, GPIO_PIN_10}, \
    {GPIOD, GPIO_PIN_0},  {GPIOD, GPIO_PIN_3},  {GPIOD, GPIO_PIN_6}  }

#define EXV_DIR_PINS  { \
    {GPIOE, GPIO_PIN_14}, {GPIOB, GPIO_PIN_3},  {GPIOA, GPIO_PIN_15}, \
    {GPIOC, GPIO_PIN_12}, {GPIOD, GPIO_PIN_2},  {GPIOD, GPIO_PIN_5}  }

#define EXV_EN_PINS   { \
    {GPIOE, GPIO_PIN_15}, {GPIOB, GPIO_PIN_5},  {GPIOC, GPIO_PIN_11}, \
    {GPIOD, GPIO_PIN_1},  {GPIOD, GPIO_PIN_4},  {GPIOD, GPIO_PIN_7}  }

// ─── Octovalve ────────────────────────────────────────────────────────────────
#define OCTO_IN1_PORT         GPIOC
#define OCTO_IN1_PIN          GPIO_PIN_13

// Encoder pulse — remapped to PE12 (PA8 not on HOPE G474VET6 headers)
#define OCTO_PULSE_PORT       GPIOE
#define OCTO_PULSE_PIN        GPIO_PIN_12
#define OCTO_PULSE_EXTI_LINE  EXTI_LINE_12
#define OCTO_PULSE_IRQn       EXTI15_10_IRQn

#define OCTO_IN1_PORT         GPIOC
#define OCTO_IN1_PIN          GPIO_PIN_13
// PC14 not on HOPE G474VET6 headers — remapped to PB10
#define OCTO_IN2_PORT         GPIOB
#define OCTO_IN2_PIN          GPIO_PIN_10

// Octovalve positions (5 valid positions, pulse-count centered)
// Spec document Part 3: 0=SERIES, 1=SERIES_RAD_BYP, 2=PARALLEL,
//                       3=AMBIENT_SOURCE, 4=COP1
#define OCTO_POS_COUNT        5
#define OCTO_TOTAL_PULSES     950
// pos_centers[1..5]: pulse counts at center of each position
// index 0 unused; positions numbered 1-5
#define OCTO_POS_CENTERS      {0, 0, 240, 450, 670, 950}
#define OCTO_POSITION_TOL     10    // ±10 pulses tolerance
#define OCTO_STALL_MS         200   // ms without movement = stalled

typedef enum {
    OCTO_SERIES         = 1,   // Pos 0 in spec — battery+PT → radiator
    OCTO_SERIES_RAD_BYP = 2,   // Pos 1 in spec — battery+PT → chiller bypass
    OCTO_PARALLEL       = 3,   // Pos 2 in spec — loops separated
    OCTO_AMBIENT_SOURCE = 4,   // Pos 3 in spec — ambient heat via radiator
    OCTO_COP1           = 5,   // Pos 4 in spec — pumps OFF, mister loop
    OCTO_UNKNOWN        = 0,
} OctoPos_t;

// Legacy aliases for valves.c compatibility
#define OCTO_POS1           OCTO_SERIES
#define OCTO_POS2_SERIES    OCTO_SERIES_RAD_BYP
#define OCTO_POS3_AMBIENT   OCTO_PARALLEL
#define OCTO_POS4_RBYPASS   OCTO_AMBIENT_SOURCE
#define OCTO_POS5_PARALLEL  OCTO_COP1

#define CLOCKWISE            true
#define COUNTERCLOCKWISE     false

// ─── Evaporator shut-off valve (GPIO, active HIGH) ────────────────────────────
// Spec doc: "Refrigerant evap Shut off valve" — 2 wires (Power in, GND)
#define VALVE_EVAP_SOV_PORT  GPIOE
#define VALVE_EVAP_SOV_PIN   GPIO_PIN_11

// Legacy alias used in thermal_control.c
#define SOLENOID_PORT        VALVE_EVAP_SOV_PORT
#define SOLENOID_PIN         VALVE_EVAP_SOV_PIN

// ─── Coolant pumps (TIM4 CH1/CH2, 10 Hz PWM) ─────────────────────────────────
#define PUMP_TIM              TIM4
#define PUMP_BATT_TIM_CH      TIM_CHANNEL_1    // PB6 AF2
#define PUMP_PT_TIM_CH        TIM_CHANNEL_2    // PB7 AF2
#define PUMP_TIM_PERIOD       (50000 - 1)
#define PUMP_TIM_PSC          (340 - 1)        // 170 MHz / 340 = 500 kHz
#define PUMP_MAX_DUTY_PCT     80
#define PUMP_RPM_FROM_TICKS(t) (30000000.0f / (t))

// ─── Fan (TIM2_CH4, PB11, 100 Hz) ────────────────────────────────────────────
#define FAN_TIM               TIM2
#define FAN_TIM_CH            TIM_CHANNEL_4
#define FAN_TIM_PERIOD        (65535)
#define FAN_TIM_PSC           (25)

// ─── FDCAN1 (AC compressor, 500 kbit/s) ──────────────────────────────────────
// Real IDs verified from Superman-Firmware source (pumps.cpp / interface.cpp)
#define CAN_ID_COMP_CMD         0x281   // TX: duty (0.1%) + power limit + enable
#define CAN_ID_COMP_STATUS_227  0x227   // RX: RPM, duty, temp, faults
#define CAN_ID_COMP_STATUS_2A8  0x2A8   // RX: HV power, current, voltage
#define CAN_ID_HP_TELEMETRY_1   0x732   // TX: pressures + coolant temps
#define CAN_ID_HP_TELEMETRY_2   0x733   // TX: misc temps + octovalve + comp
#define CAN_TX_PERIOD_MS        100     // 10 Hz

// ─── ADC DMA buffer indices ───────────────────────────────────────────────────
//
//  ADC1 — 5 channels → g_adc1Dma[0..4]
//  (CubeMX config: PA0=IN1, PA1=IN2, PA2=IN3, PA3=IN4, PB0=IN15)
//
#define ADC1_IDX_PT_DISC_PRES    0   // PA0 — PT_discharge pressure (high side)
#define ADC1_IDX_PT_SC_PRES      1   // PA1 — PT_SC pressure (subcool/liquid)
#define ADC1_IDX_COOLANT_LEVEL   2   // PA2 — coolant level (capacitive)
#define ADC1_IDX_OCTO_SENSE      3   // PA3 — octovalve Hall sensor (2-wire current sense)
#define ADC1_IDX_COOLANT_BATT    4   // PB0 — coolant temp battery inlet (NTC)
#define ADC1_BUF_COUNT           5

//  ADC2 — 8 channels → g_adc2Dma[0..7]
//  (CubeMX config: PA4=IN17, PA5=IN13, PC0=IN6..PC5=IN11)
//
#define ADC2_IDX_RESERVED_B      0   // PA4 — reserved (future cabin temp)
#define ADC2_IDX_RESERVED_C      1   // PA5 — reserved (future cabin temp)
#define ADC2_IDX_PT_DISC_TEMP    2   // PC0 — PT_discharge temp (high side)
#define ADC2_IDX_PT_SUCT_TEMP    3   // PC1 — PT_suction temp (low side)
#define ADC2_IDX_PT_SC_TEMP      4   // PC2 — PT_SC temp (subcool)
#define ADC2_IDX_PT_SUCT_PRES    5   // PC3 — PT_suction pressure (low side)
#define ADC2_IDX_UAUX            6   // PC4 — 12V supply monitor
#define ADC2_IDX_COOLANT_PT      7   // PC5 — coolant temp powertrain inlet (NTC)
#define ADC2_BUF_COUNT           8

#define ADC_MAX_RAW     4095    // 12-bit max raw value
#define ADC_FULL_SCALE  4096    // full scale (divisor)

// ─── PT sensor scaling (ratiometric, 5V Vref) ─────────────────────────────────
//  PT sensors output 0.5V–4.5V ratiometric to 5V Vref.
//  Voltage divider: 10kΩ + 10kΩ → ratio 0.5 → ADC sees 0.25V–2.25V.
//
//  Recovery formula:
//    V_adc    = (raw / 4095.0) × 3.3
//    V_sensor = V_adc / PT_DIVIDER_RATIO          (reverse the divider)
//    Pressure = (V_sensor - PT_V_MIN) / PT_V_SPAN × full_scale_bar
//    Temp     = (V_sensor - PT_V_MIN) / PT_V_SPAN × temp_span + temp_min
//
#define PT_DIVIDER_RATIO     0.5f    // 10k / (10k + 10k)
#define PT_V_MIN             0.5f    // sensor output at zero (V)
#define PT_V_MAX             4.5f    // sensor output at full scale (V)
#define PT_V_SPAN            4.0f    // PT_V_MAX - PT_V_MIN

// Pressure full-scale values (from Superman-Firmware sensors.cpp)
#define PT_DISC_BAR_MAX      37.265f  // high side (discharge)
#define PT_SUCT_BAR_MAX      10.859f  // low side (suction)
#define PT_SC_BAR_MAX        10.859f  // subcool (liquid line)

// PT sensor temperature ranges
// ⚠ Verify these ranges against Tesla PT sensor datasheet during testing
// Discharge sensor: -40°C to +150°C
// Suction / subcool: -40°C to +80°C
#define PT_DISC_TEMP_MIN    -40.0f
#define PT_DISC_TEMP_SPAN   190.0f   // 150 - (-40)
#define PT_SUCT_TEMP_MIN    -40.0f
#define PT_SUCT_TEMP_SPAN   120.0f   // 80 - (-40)
#define PT_SC_TEMP_MIN      -40.0f
#define PT_SC_TEMP_SPAN     120.0f

// ─── Coolant level sensor ─────────────────────────────────────────────────────
//  Capacitive sensor on PA2, 2-wire (Sensor in, GND).
//  Output assumed 0–3.3V analog (verify against actual sensor during testing).
//  0.0 = empty, 1.0 = full
#define COOLANT_LEVEL_FULL_V   3.3f

// ─── uaux 12V monitor ─────────────────────────────────────────────────────────
// Voltage divider gain from Superman-Firmware main.cpp
#define UAUX_GAIN              203.0f

// ─── Digital inputs ───────────────────────────────────────────────────────────
#define DIN_CABIN_HEATR_PORT   GPIOE
#define DIN_CABIN_HEATR_PIN    GPIO_PIN_8
#define DIN_CABIN_COOL_PORT    GPIOE
#define DIN_CABIN_COOL_PIN     GPIO_PIN_9
#define DIN_CABIN_HEATL_PORT   GPIOE
#define DIN_CABIN_HEATL_PIN    GPIO_PIN_10
#define DIN_PREHEAT_PORT       GPIOB
#define DIN_PREHEAT_PIN        GPIO_PIN_2

// ─── Thermal control constants (from spec document) ───────────────────────────
// COP mode thresholds (with ±1°C hysteresis applied in thermal_control.c)
#define COP_COLD_START_THRESH  -30.0f  // bat below this → COLD_START
#define COP_1_THRESH            10.0f  // bat below this → COP_1
#define COP_BLEND_THRESH        15.0f  // bat below this → COP_BLEND
#define COP_HIGH_MAX            50.0f  // bat above this → throttled COP_HIGH

// Safety limits (spec Part 7)
#define PT_HIGH_WARN_BAR       30.0f   // reduce compressor speed
#define PT_HIGH_CRIT_BAR       33.0f   // shutdown compressor
#define PT_HIGH_TEMP_CRIT_C   140.0f   // shutdown compressor
#define PT_LOW_MIN_BAR          1.0f   // possible low charge alarm
#define EXV_RECIRC_MAX_STEPS  140      // 70% of 200 — hard cap in COP_1

// Subcool PID targets (spec Part 6.1)
#define SUBCOOL_TARGET_MIN_C    5.0f
#define SUBCOOL_TARGET_MAX_C   10.0f

// Suction pressure PID targets (spec Part 6.3, COP_1/BLEND only)
#define SUCTION_PRES_MIN_BAR    2.0f
#define SUCTION_PRES_MAX_BAR    4.0f

// Battery thermal thresholds (spec Part 4.3 — legacy, used in thermal_control)
#define BATTERY_HEAT_THRESHOLD   5.0f
#define BATTERY_COOL_THRESHOLD  40.0f
#define POWERTRAIN_COOL_THRESH  50.0f

// ─── System task timing ───────────────────────────────────────────────────────
#define TASK_EXV_STEP_MS         1    // TIM6 ISR
#define TASK_FAST_MS            10    // octovalve, I/O, pump feedback
#define TASK_SLOW_MS           100    // thermal control, CAN TX, sensors, WDT

// ─── Compressor startup ramp (spec Part 6.4) ─────────────────────────────────
#define COMP_MIN_DUTY_PCT       20    // minimum speed (oil starvation limit)
#define COMP_RAMP_STEPS_MS    5000    // ramp from 20% over 5 seconds

// ─── EXV rate limit (spec Part 9.1) ──────────────────────────────────────────
#define EXV_MAX_STEP_CHANGE     10    // max steps per 100ms control cycle

// ─── Flash settings ───────────────────────────────────────────────────────────
#define SETTINGS_FLASH_ADDR   0x0807F800UL
#define SETTINGS_FLASH_BANK   FLASH_BANK_2
#define SETTINGS_FLASH_PAGE   127
#define SETTINGS_MAGIC        0xDEAD4750UL
#define SETTINGS_VERSION      1

// ─── IWDG ─────────────────────────────────────────────────────────────────────
#define IWDG_PRESCALER_VAL    IWDG_PRESCALER_64
#define IWDG_RELOAD_VAL       2499   // ~5s at LSI 32 kHz

// ─── Legacy aliases for thermal_control.c and valves.c compatibility ──────────
#define OCTO_POSITION_TOLERANCE  OCTO_POSITION_TOL
#define RECIRC_TEMP_THRESHOLD   -20.0f
#define HIGH_PRESSURE_LIMIT      PT_HIGH_WARN_BAR
#define LOW_PRESSURE_LIMIT       PT_LOW_MIN_BAR
#define MIN_VALVE_POSITION       5.0f
#define CONDENSOR_SETP_MIN       50
#define CONDENSOR_SETP_MAX       70
#define EVAP_SETP_MIN            5
#define EVAP_SETP_MAX            10