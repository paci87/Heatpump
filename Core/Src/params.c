#include "main.h"
#include "params.h"
#include "setpoint.h"
#include "stm32g4xx_hal.h"
#include <string.h>

// ─── In-RAM parameter table ───────────────────────────────────────────────────
// We keep a flat array indexed by ParamId_t for fast access.
// IDs above 2000 are display values; we remap them to avoid a huge array.

typedef struct { int32_t ival; float fval; } ParamEntry_t;

#define SAVEABLE_COUNT  11   // IDs 0-10
#define DISPLAY_BASE    2000
#define DISPLAY_COUNT   53   // IDs 2000-2052

static int32_t _saveable[SAVEABLE_COUNT];
static float   _display[DISPLAY_COUNT];

// Default values for saveable params (order matches ParamId_t 0-10)
static const int32_t _defaults[SAVEABLE_COUNT] = {
    2,     // canspeed: 500k
    50,    // nodeid
    0,     // canio: HW_IO
    20,    // temp_battery_setp
    0,     // temp_battery_min
    50,    // temp_battery_max
    0,     // temp_powertrain_min
    50,    // temp_powertrain_max
    5,     // temp_evaporator_setp
    70,    // temp_condensor_setp
    6000,  // compressor_plim (W)
};

// ─── Flash settings block ─────────────────────────────────────────────────────
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t crc;
    int32_t  saveable[SAVEABLE_COUNT];
} FlashSettings_t;

static uint32_t _crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void Params_Init(void) {
    // Load defaults
    for (int i = 0; i < SAVEABLE_COUNT; i++) _saveable[i] = _defaults[i];
    memset(_display, 0, sizeof(_display));
    Params_SetInt(PARAM_cabin_setp_l, 21);
    Params_SetInt(PARAM_cabin_setp_r, 21);
    Params_SetInt(PARAM_cabin_fan_pwm, 0);
    Params_SetInt(PARAM_opmode, OPMODE_GPIO);
    Params_Load();
}

void Params_Load(void) {
    const FlashSettings_t *f = (const FlashSettings_t *)SETTINGS_FLASH_ADDR;
    if (f->magic != SETTINGS_MAGIC || f->version != SETTINGS_VERSION) return;
    uint32_t crc = _crc32((const uint8_t *)f->saveable, sizeof(f->saveable));
    if (crc != f->crc) return;
    memcpy(_saveable, f->saveable, sizeof(_saveable));
}

void Params_Save(void) {
    FlashSettings_t s;
    s.magic   = SETTINGS_MAGIC;
    s.version = SETTINGS_VERSION;
    memcpy(s.saveable, _saveable, sizeof(_saveable));
    s.crc = _crc32((const uint8_t *)s.saveable, sizeof(s.saveable));

    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = { FLASH_TYPEERASE_PAGES, SETTINGS_FLASH_BANK,
                                     SETTINGS_FLASH_PAGE, 1 };
    uint32_t pageErr = 0;
    HAL_FLASHEx_Erase(&erase, &pageErr);

    const uint64_t *src  = (const uint64_t *)&s;
    uint32_t        addr = SETTINGS_FLASH_ADDR;
    for (uint32_t i = 0; i < (sizeof(s) + 7) / 8; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, src[i]);
        addr += 8;
    }
    HAL_FLASH_Lock();
}

// ─── Accessors ────────────────────────────────────────────────────────────────
static inline int _dispIdx(ParamId_t id) { return (int)id - DISPLAY_BASE; }

void Params_SetInt(ParamId_t id, int32_t val) {
    if (id < SAVEABLE_COUNT)           _saveable[id] = val;
    else if ((int)id >= DISPLAY_BASE)  _display[_dispIdx(id)] = (float)val;
}

void Params_SetFloat(ParamId_t id, float val) {
    if (id < SAVEABLE_COUNT)           _saveable[id] = (int32_t)val;
    else if ((int)id >= DISPLAY_BASE)  _display[_dispIdx(id)] = val;
}

int32_t Params_GetInt(ParamId_t id) {
    if (id < SAVEABLE_COUNT)           return _saveable[id];
    if ((int)id >= DISPLAY_BASE)       return (int32_t)_display[_dispIdx(id)];
    return 0;
}

float Params_GetFloat(ParamId_t id) {
    if (id < SAVEABLE_COUNT)           return (float)_saveable[id];
    if ((int)id >= DISPLAY_BASE)       return _display[_dispIdx(id)];
    return 0.0f;
}

bool Params_GetBool(ParamId_t id) { return Params_GetInt(id) != 0; }
