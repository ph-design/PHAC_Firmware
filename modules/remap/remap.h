#pragma once

#include "config.h"
#include "hardware/flash.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_CONFIG_MAGIC 0x55AA1234

#define REMAP_CONFIG_SIZE sizeof(RemapConfig)

typedef struct
{
    uint8_t keymap_keyboard[BUTTON_COUNT];
    uint8_t keymap_gamepad[BUTTON_COUNT];
    RGBColor button_colors[BUTTON_COUNT];
    float brightness;
    uint16_t anim_speed;
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
bool remap_handle_command(const uint8_t *data, uint16_t len,
                          uint8_t *response, uint16_t *response_len);
void remap_get_raw_config(uint8_t *buffer, size_t max_len);
void remap_save_config(void);
