#pragma once

#include "config.h"
#include "hardware/flash.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_CONFIG_MAGIC 0x55AA1234

#define REMAP_CONFIG_SIZE sizeof(RemapConfig)

// START button protection time options
typedef enum
{
    START_PROTECTION_DISABLED = 0,
    START_PROTECTION_100MS = 100,
    START_PROTECTION_250MS = 250,
    START_PROTECTION_500MS = 500
} StartProtectionTime;

typedef struct
{
    uint8_t keymap_keyboard[BUTTON_COUNT];
    uint8_t keymap_gamepad[BUTTON_COUNT];
    RGBColor button_colors[BUTTON_COUNT];
    float brightness;
    uint16_t anim_speed;
    
    // START button protection settings
    uint16_t start_protection_ms;  // Protection time: 0/100/250/500
    bool start_macro_enabled;      // Enable START macro feature
    uint8_t start_macro_trigger_key;  // Which key to press before password (placeholder)
    uint8_t start_macro_password[4];  // 4-digit password, default: 1,2,3,4
} RemapConfig;

#pragma pack(push, 1)
typedef struct
{
    uint32_t magic;
    RemapConfig config;
} StoredConfig;
#pragma pack(pop)

void remap_init(void);
const RemapConfig *remap_get_config(void);
bool remap_process_command(const uint8_t *data, uint16_t len);
void remap_get_raw_config(uint8_t *buffer, size_t max_len);
void remap_get_firmware_version(uint8_t *buffer, size_t max_len);
void remap_save_config(void);
