#pragma once

#include "tusb.h"

// Button configurations
#define BUTTON_COUNT 7
#define BTN_BTA 6
#define BTN_BTB 5
#define BTN_BTC 4
#define BTN_BTD 3
#define BTN_FXL 2
#define BTN_START 1
#define BTN_FXR 0

// Encoder configurations
#define ENCODER_X_PIN_A 9
#define ENCODER_X_PIN_B 10
#define ENCODER_Y_PIN_A 7
#define ENCODER_Y_PIN_B 8

// Default keymaps
static const uint8_t default_keymap_keyboard_mode[BUTTON_COUNT] = {
    HID_KEY_D, HID_KEY_F, HID_KEY_J, HID_KEY_K, HID_KEY_N, HID_KEY_Y, HID_KEY_M};

static const uint8_t default_keymap_gamepad_mode[BUTTON_COUNT] = {
    0, 1, 2, 3, 4, 5, 6};

// Default RGB colors
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGBColor;

static const RGBColor default_button_colors[BUTTON_COUNT] = {
    {212, 93, 153}, // A
    {212, 93, 153}, // B
    {212, 93, 153}, // C
    {212, 93, 153}, // D
    {0, 47, 167},   // fxL
    {255, 10, 10},  // START
    {0, 47, 167}    // fxR
};

// Default settings
#define DEFAULT_BRIGHTNESS 0.1
#define DEFAULT_ANIM_SPEED 100

// Memory offsets,don't change this unless you know what you're doing
#define FLASH_SECTOR_SIZE (1u << 12)
#define SYSTEM_CONFIG_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define REMAP_CONFIG_OFFSET (PICO_FLASH_SIZE_BYTES - 2 * FLASH_SECTOR_SIZE)