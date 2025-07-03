#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "hardware/gpio.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include "modules/encoder/ec11.h"
#include "modules/debounce/debounce.h"
#include "modules/rgb/ws2812.h"
#include "math.h"
#include "ws2812.pio.h"

//--------------------------------------------------------------------+
// Hardware Configuration
//--------------------------------------------------------------------+

// Debounce default 5ms
#undef DEBOUNCE_TIME_MS
#define DEBOUNCE_TIME_MS 5

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
#define ENCODER_X_PIN_A 10
#define ENCODER_X_PIN_B 9
#define ENCODER_Y_PIN_A 7
#define ENCODER_Y_PIN_B 8

// WS2812 LED configurations
#define WS2812_PIN 11
#define DEFAULT_BRIGHTNESS 0.4 // 10% brightness
#define RGB_COUNT 7
#define IS_RGBW false // RGB mode, not RGBW

// Flash storage configuration
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_CONFIG_MAGIC 0xABCD1234

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// System operation modes
typedef enum
{
	MODE_BOOTLOADER,
	MODE_KEYBOARD,
	MODE_GAMEPAD
} SystemMode;

typedef struct
{
	uint32_t magic;
	SystemMode mode;
} SystemConfig;

// USB interface IDs
enum
{
	ITF_KEYBOARD = 0,
	ITF_MOUSE = 1,
	ITF_GAMEPAD = 2,
	ITF_GENERIC = 3,
};

// LED blink rates
enum
{
	BLINK_KEYBOARD_MODE = 500,
	BLINK_GAMEPAD_MODE = 200,
	BLINK_NOT_MOUNTED = 250,
	BLINK_MOUNTED = 1000,
	BLINK_SUSPENDED = 2500,
};
static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static bool led_state = false;
static char response_buf[64];

void led_blinking_task(void);

// Mouse parameters
#define ENCODER_BASE_SENSITIVITY 5		 // Base encoder sensitivity
#define MOUSE_SENSITIVITY_MULTIPLIER 2 // Mouse movement multiplier
#define GAMEPAD_SENSITIVITY 10				 // Gamepad axis sensitivity
#define SMOOTHING_FACTOR 5.0f					 // Smoothing factor for mouse movement
#define MAIN_LOOP_INTERVAL_MS 0.1			 // HID report interval

// Keycode mappings
static const uint8_t button_pins[BUTTON_COUNT] = {
		BTN_BTA,
		BTN_BTB,
		BTN_BTC,
		BTN_BTD,
		BTN_FXL,
		BTN_START,
		BTN_FXR};

static const uint8_t KEYMAP_KEYBOARD_MODE[BUTTON_COUNT] = {
		HID_KEY_D, // BTN_BTA
		HID_KEY_F, // BTN_BTB
		HID_KEY_J, // BTN_BTC
		HID_KEY_K, // BTN_BTD
		HID_KEY_N, // BTN_FXL
		HID_KEY_Y, // BTN_START
		HID_KEY_M	 // BTN_FXR
};

static const uint8_t KEYMAP_GAMEPAD_MODE[BUTTON_COUNT] = {
		0, // BTN_BTA -> Button 1
		1, // BTN_BTB -> Button 2
		2, // BTN_BTC -> Button 3
		3, // BTN_BTD -> Button 4
		4, // BTN_FXL -> Button 5
		5, // BTN_START -> Button 6
		6	 // BTN_FXR -> Button 7
};

// HID Report Echo variables
static uint8_t received_data[64];
static uint8_t received_report_id = 0;
static uint8_t received_itf = 0;
static uint16_t received_size = 0;
static bool send_response = false;

//--------------------------------------------------------------------+
// Globals
//--------------------------------------------------------------------+
EC11_Encoder encoder_x, encoder_y;
DebounceButton debounce_buttons[BUTTON_COUNT];

static int8_t mouse_x = 0;
static int8_t mouse_y = 0;

static uint16_t gamepad_x = 0;
static uint16_t gamepad_y = 0;

static uint32_t prev_btn_state = 0;

static float remaining_delta_x = 0.0f;
static float remaining_delta_y = 0.0f;

static SystemMode current_mode = MODE_KEYBOARD;

//--------------------------------------------------------------------+
// Encoder callbacks
//---------------------------------------------------------------------
void encoder_x_callback(EC11_Direction dir, void *user_data)
{
	if (current_mode == MODE_GAMEPAD)
	{
		gamepad_x = (gamepad_x + dir * GAMEPAD_SENSITIVITY) % 512; // 512 cycle period (2 full rotations)
	}
	else
	{
		remaining_delta_x += dir * ENCODER_BASE_SENSITIVITY;
	}
}

void encoder_y_callback(EC11_Direction dir, void *user_data)
{
	if (current_mode == MODE_GAMEPAD)
	{
		gamepad_y = (gamepad_y + dir * GAMEPAD_SENSITIVITY) % 512;
	}
	else
	{
		remaining_delta_y += dir * ENCODER_BASE_SENSITIVITY;
	}
}

void hid_task(void);
static void send_keyboard_report(uint32_t btn);

SystemMode load_system_mode(void);
void save_system_mode(SystemMode mode);

static void handle_rawhid_response(void);

//--------------------------------------------------------------------+
// Main application
//---------------------------------------------------------------------
int main(void)
{
	// Hardware initialization
	board_init();
	tusb_rhport_init_t dev_init = {
			.role = TUSB_ROLE_DEVICE,
			.speed = TUSB_SPEED_AUTO};
	tusb_init(BOARD_TUD_RHPORT, &dev_init);
	if (board_init_after_tusb)
	{
		board_init_after_tusb();
	}
	current_mode = load_system_mode();

	// Initialize buttons with debouncing
	debounce_init(debounce_buttons, button_pins, BUTTON_COUNT);

	sleep_ms(10); // Initial debounce delay
	bool mode_changed = false;
	SystemMode new_mode = current_mode;

	if (!gpio_get(BTN_START))
	{
		reset_usb_boot(0, 0); // Enter bootloader without saving
	}
	else
	{
		// Check mode selection buttons
		if (!gpio_get(BTN_BTA))
		{
			new_mode = MODE_KEYBOARD;
			mode_changed = true;
		}
		else if (!gpio_get(BTN_BTB))
		{
			new_mode = MODE_GAMEPAD;
			mode_changed = true;
		}

		if (mode_changed && new_mode != current_mode)
		{
			current_mode = new_mode;
			save_system_mode(current_mode); // Persist new mode
		}
	}

	// Initialize encoders
	ec11_init(&encoder_x, ENCODER_X_PIN_A, ENCODER_X_PIN_B, encoder_x_callback, NULL);
	ec11_init(&encoder_y, ENCODER_Y_PIN_A, ENCODER_Y_PIN_B, encoder_y_callback, NULL);

	// Initialize ws2812
	ws2812_t strip;

	// 自动分配PIO资源
	PIO pio = pio0;
	uint sm = pio_claim_unused_sm(pio, true);

	// 初始化灯带 (800kHz频率)
	if (!ws2812_init(&strip, pio, sm, WS2812_PIN, RGB_COUNT, 800000))
	{
		printf("Failed to initialize WS2812!\n");
		return 1;
	}

	int t = 0;

	while (1)
	{
		tud_task(); // Handle USB events
		handle_rawhid_response();
		led_blinking_task();
		ec11_update(&encoder_x);
		ec11_update(&encoder_y);
		debounce_update(debounce_buttons, BUTTON_COUNT);
		hid_task(); // Process HID reports
		ws2812_set_pixel(&strip, 0, 10, 0, 50);
		ws2812_show(&strip);
		
	}
}

//--------------------------------------------------------------------+
// HID implementation
//---------------------------------------------------------------------
static uint32_t read_buttons(void)
{
	return debounce_get_states(debounce_buttons, BUTTON_COUNT);
}

static void send_keyboard_report(uint32_t btn)
{
	if (!tud_hid_n_ready(ITF_KEYBOARD))
		return;

	static uint32_t prev_btn_state = 0;
	if (btn == prev_btn_state)
		return;
	prev_btn_state = btn;

	uint8_t keycode[6] = {0};
	uint8_t key_cnt = 0;

	for (int i = 0; i < BUTTON_COUNT && key_cnt < 6; i++)
	{
		if (btn & (1 << i))
		{
			keycode[key_cnt++] = KEYMAP_KEYBOARD_MODE[i];
		}
	}
	tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, key_cnt ? keycode : NULL);
}

static void handle_keyboard_mouse_mode(uint32_t btn_state)
{
	send_keyboard_report(btn_state);

	int8_t step_x = 0;
	int8_t step_y = 0;

	// X-axis processing
	if (fabsf(remaining_delta_x) >= 1.0f)
	{
		const float ideal_step = remaining_delta_x / SMOOTHING_FACTOR;
		int8_t quantized_step = (int8_t)roundf(ideal_step);

		if (fabsf(quantized_step) > fabsf(remaining_delta_x))
		{
			quantized_step = (remaining_delta_x > 0) ? 1 : -1;
		}

		remaining_delta_x -= quantized_step;
		step_x = quantized_step * MOUSE_SENSITIVITY_MULTIPLIER;
	}

	// Y-axis processing
	if (fabsf(remaining_delta_y) >= 1.0f)
	{
		const float ideal_step = remaining_delta_y / SMOOTHING_FACTOR;
		int8_t quantized_step = (int8_t)roundf(ideal_step);

		if (fabsf(quantized_step) > fabsf(remaining_delta_y))
		{
			quantized_step = (remaining_delta_y > 0) ? 1 : -1;
		}

		remaining_delta_y -= quantized_step;
		step_y = quantized_step * MOUSE_SENSITIVITY_MULTIPLIER;
	}

	if ((step_x != 0 || step_y != 0) && tud_hid_n_ready(ITF_MOUSE))
	{
		tud_hid_n_mouse_report(ITF_MOUSE, 0, 0x00, step_x, step_y, 0, 0);
	}
}

static void handle_gamepad_mode(uint32_t btn_state)
{
	uint32_t gamepad_buttons = 0;
	for (int i = 0; i < BUTTON_COUNT; i++)
	{
		if (btn_state & (1 << i))
		{
			gamepad_buttons |= (1 << KEYMAP_GAMEPAD_MODE[i]);
		}
	}

	// Map encoder positions to gamepad axes
	int16_t mapped_x = (int16_t)gamepad_x * 255 / 256 - 255;
	int16_t mapped_y = (int16_t)gamepad_y * 255 / 256 - 255;

	int8_t axis_x = (int8_t)(mapped_x);
	int8_t axis_y = (int8_t)(mapped_y);

	if (tud_hid_n_ready(ITF_GAMEPAD))
	{
		tud_hid_n_gamepad_report(ITF_GAMEPAD, 0,
														 axis_x, // X
														 axis_y, // Y
														 0,			 // Z
														 0,			 // Rz
														 0,			 // Rx
														 0,			 // Ry
														 0,			 // Hat
														 gamepad_buttons);
	}
}

static void handle_rawhid_response(void)
{
	if (send_response && tud_hid_n_ready(ITF_GENERIC))
	{
		tud_hid_n_report(ITF_GENERIC, received_report_id, received_data, received_size);
		send_response = false;
	}
}

void hid_task(void)
{
	static uint32_t start_ms = 0;
	const uint32_t current_time = board_millis();

	if (current_time - start_ms < MAIN_LOOP_INTERVAL_MS)
		return;
	start_ms = current_time;

	const uint32_t btn_state = read_buttons();

	if (tud_suspended())
	{
		if (btn_state)
			tud_remote_wakeup();
		return;
	}

	switch (current_mode)
	{
	case MODE_KEYBOARD:
		handle_keyboard_mouse_mode(btn_state);
		break;
	case MODE_GAMEPAD:
		handle_gamepad_mode(btn_state);
		break;
	default:
		break;
	}
}

//--------------------------------------------------------------------+
// Flash storage operations
//---------------------------------------------------------------------
SystemMode load_system_mode(void)
{
	const SystemConfig *config = (const SystemConfig *)(XIP_BASE + FLASH_TARGET_OFFSET);
	if (config->magic == FLASH_CONFIG_MAGIC)
	{
		return config->mode;
	}
	return MODE_KEYBOARD; // Default mode
}

void save_system_mode(SystemMode mode)
{
	SystemConfig config = {
			.magic = FLASH_CONFIG_MAGIC,
			.mode = mode,
	};

	uint8_t sector[FLASH_SECTOR_SIZE] = {0};
	memcpy(sector, &config, sizeof(config));

	// Safe flash operations
	uint32_t ints = save_and_disable_interrupts();
	flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FLASH_TARGET_OFFSET, sector, FLASH_SECTOR_SIZE);
	restore_interrupts(ints);
}

//--------------------------------------------------------------------+
// USB HID callbacks
//---------------------------------------------------------------------
void tud_mount_cb(void)
{
	blink_interval_ms = BLINK_MOUNTED;
}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool remote_wakeup_en)
{
	(void)remote_wakeup_en;
}
void tud_resume_cb(void) {}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
															 hid_report_type_t report_type, uint8_t *buffer,
															 uint16_t reqlen)
{
	(void)itf;
	(void)report_id;
	(void)report_type;
	(void)buffer;
	(void)reqlen;
	return 0;
}

void tud_hid_set_report_cb(
		uint8_t itf,
		uint8_t report_id,
		hid_report_type_t report_type,
		uint8_t const *buffer,
		uint16_t bufsize)
{
	(void)report_type;

	if (itf == ITF_GENERIC)
	{
		const char *prefix = "pico received: ";
		size_t prefix_len = strlen(prefix); // 计算前缀长度
		size_t total_available = sizeof(received_data);

		// 确定实际可复制的前缀长度
		size_t prefix_copy_len = (prefix_len <= total_available) ? prefix_len : total_available;

		// 剩余空间用于数据部分
		size_t data_available = total_available - prefix_copy_len;
		size_t data_copy_size = (bufsize <= data_available) ? bufsize : data_available;

		// 更新接收数据的总大小
		received_size = prefix_copy_len + data_copy_size;

		// 复制前缀到接收缓冲区
		memcpy(received_data, prefix, prefix_copy_len);

		// 复制数据部分到前缀之后
		if (data_copy_size > 0)
		{
			memcpy(received_data + prefix_copy_len, buffer, data_copy_size);
		}

		received_report_id = report_id;
		received_itf = itf;
		send_response = true;
	}
}

void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint16_t len)
{
	(void)itf;
	(void)report;
	(void)len;
}

void led_blinking_task(void)
{
	static uint32_t start_ms = 0;

	if (board_millis() - start_ms < blink_interval_ms)
		return;
	start_ms = board_millis();

	board_led_write(led_state);
	led_state = !led_state;
}