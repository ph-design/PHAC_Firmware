// remap.c
#include "remap.h"
#include "ws2812.h"
#include <string.h>

static RemapConfig current_config;

void remap_init(void)
{
    const StoredConfig *stored = (const StoredConfig *)(XIP_BASE + REMAP_CONFIG_OFFSET);

    if (stored->magic == FLASH_CONFIG_MAGIC)
    {
        // Load configuration from flash
        memcpy(&current_config, &stored->config, sizeof(RemapConfig));
    }
    else
    {
        // Use default configuration
        memcpy(current_config.keymap_keyboard, default_keymap_keyboard_mode,
               sizeof(default_keymap_keyboard_mode));
        memcpy(current_config.keymap_gamepad, default_keymap_gamepad_mode,
               sizeof(default_keymap_gamepad_mode));
        memcpy(current_config.button_colors, default_button_colors,
               sizeof(default_button_colors));
        current_config.brightness = DEFAULT_BRIGHTNESS;
        current_config.anim_speed = DEFAULT_ANIM_SPEED;
    }

    // Apply configuration
    ws2812_set_brightness(current_config.brightness);
}

const RemapConfig *remap_get_config(void)
{
    return &current_config;
}

void remap_get_raw_config(uint8_t *buffer, size_t max_len)
{
    const RemapConfig *config = remap_get_config();
    size_t copy_len = sizeof(RemapConfig);

    if (copy_len > max_len)
        copy_len = max_len;
    memcpy(buffer, config, copy_len);
}

bool remap_process_command(const uint8_t *data, uint16_t len)
{
    if (len < 2)
        return false; // Need at least command type and length

    uint8_t cmd_type = data[0];
    uint8_t cmd_len = data[1];

    if (len < (2 + cmd_len))
        return false;

    const uint8_t *payload = data + 2;

    switch (cmd_type)
    {
    case 0x01: // Remap keyboard key value
        if (cmd_len == BUTTON_COUNT)
        {
            memcpy(current_config.keymap_keyboard, payload, BUTTON_COUNT);
            return true;
        }
        break;

    case 0x02: // Remap gamepad key value
        if (cmd_len == BUTTON_COUNT)
        {
            memcpy(current_config.keymap_gamepad, payload, BUTTON_COUNT);
            return true;
        }
        break;

    case 0x03: // Set RGB color
        if (cmd_len == BUTTON_COUNT * 3)
        {
            for (int i = 0; i < BUTTON_COUNT; i++)
            {
                current_config.button_colors[i].r = payload[i * 3];
                current_config.button_colors[i].g = payload[i * 3 + 1];
                current_config.button_colors[i].b = payload[i * 3 + 2];
            }
            return true;
        }
        break;

    case 0x04: // Set brightness
        if (cmd_len == 1)
        {
            current_config.brightness = payload[0] / 255.0f;
            ws2812_set_brightness(current_config.brightness);
            return true;
        }
        break;

    case 0x05: // Set animation speed
        if (cmd_len == 2)
        {
            current_config.anim_speed = (payload[0] << 8) | payload[1];
            return true;
        }
        break;

    case 0x06: // Restore default settings
        if (cmd_len == 0)
        {
            // Restore keyboard mapping
            memcpy(current_config.keymap_keyboard, default_keymap_keyboard_mode,
                   sizeof(default_keymap_keyboard_mode));
            // Restore gamepad mapping
            memcpy(current_config.keymap_gamepad, default_keymap_gamepad_mode,
                   sizeof(default_keymap_gamepad_mode));
            // Restore button colors
            memcpy(current_config.button_colors, default_button_colors,
                   sizeof(default_button_colors));
            // Restore brightness and animation speed
            current_config.brightness = DEFAULT_BRIGHTNESS;
            current_config.anim_speed = DEFAULT_ANIM_SPEED;

            // Apply brightness setting
            ws2812_set_brightness(current_config.brightness);

            // Save to Flash
            remap_save_config();

            return true;
        }
        break;

    case 0x07: // Save current configuration
        if (cmd_len == 0)   
        {
            remap_save_config();
            return true;
        }
    }

    return false;
}

void remap_save_config(void)
{
    StoredConfig stored = {
        .magic = FLASH_CONFIG_MAGIC,
        .config = current_config};

    uint8_t sector[FLASH_SECTOR_SIZE] = {0};
    memcpy(sector, &stored, sizeof(StoredConfig));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(REMAP_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(REMAP_CONFIG_OFFSET, sector, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}