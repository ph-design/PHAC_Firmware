#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "hardware/gpio.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include "drivers/encoder/ec11.h"
#include "math.h"


//--------------------------------------------------------------------+
// 硬件配置宏定义
//--------------------------------------------------------------------+

// 按钮配置
#define BUTTON_COUNT 7
#define BTN_BTA 6
#define BTN_BTB 5
#define BTN_BTC 4
#define BTN_BTD 3
#define BTN_FXL 2
#define BTN_START 1
#define BTN_FXR 0

// 编码器配置
#define ENCODER_X_PIN_A        10
#define ENCODER_X_PIN_B        9
#define ENCODER_Y_PIN_A        7
#define ENCODER_Y_PIN_B        8

//定义flash存储区
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_CONFIG_MAGIC 0xABCD1234

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// 模式定义
typedef enum {
    MODE_BOOTLOADER,
    MODE_KEYBOARD_MOUSE,
    MODE_GAMEPAD
} SystemMode;

typedef struct {
    uint32_t magic;
    SystemMode mode;
} SystemConfig;

// 鼠标参数
#define ENCODER_BASE_SENSITIVITY 5       // 编码器基础灵敏度
#define MOUSE_SENSITIVITY_MULTIPLIER 2   // 鼠标移动倍率
#define GAMEPAD_SENSITIVITY       10     // Gamepad轴灵敏度
#define SMOOTHING_FACTOR       5.0f      // 平滑系数
#define MAIN_LOOP_INTERVAL_MS 0.1         // HID报告间隔


// 键码映射
static const uint8_t button_pins[BUTTON_COUNT] = {
    BTN_BTA, BTN_BTB, BTN_BTC, BTN_BTD,
    BTN_FXL, BTN_START, BTN_FXR
};

static const uint8_t KEYMAP_MOUSE_MODE[BUTTON_COUNT] = {
    HID_KEY_D,   // BTN_BTAmn
    HID_KEY_F,   // BTN_BTB
    HID_KEY_J,   // BTN_BTC
    HID_KEY_K,   // BTN_BTD
    HID_KEY_N,   // BTN_FXL
    HID_KEY_Y,   // BTN_START
    HID_KEY_M    // BTN_FXR
};

static const uint8_t KEYMAP_GAMEPAD_MODE[BUTTON_COUNT] = {
    0,  // BTN_BTA -> Button 1
    1,  // BTN_BTB -> Button 2
    2,  // BTN_BTC -> Button 3
    3,  // BTN_BTD -> Button 4
    4,  // BTN_FXL -> Button 5
    5,  // BTN_START -> Button 6
    6   // BTN_FXR -> Button 7
};

//--------------------------------------------------------------------+
// 全局变量
//--------------------------------------------------------------------+
EC11_Encoder encoder_x, encoder_y;

static int8_t mouse_x = 0;
static int8_t mouse_y = 0;

static uint16_t gamepad_x = 0;
static uint16_t gamepad_y = 0;

static uint32_t prev_btn_state = 0;

static float remaining_delta_x = 0.0f;
static float remaining_delta_y = 0.0f;

static SystemMode current_mode = MODE_KEYBOARD_MOUSE;

//--------------------------------------------------------------------+
// 编码器回调函数
//---------------------------------------------------------------------
void encoder_x_callback(EC11_Direction dir, void* user_data) {
    if (current_mode == MODE_GAMEPAD) {
        gamepad_x = (gamepad_x + dir * GAMEPAD_SENSITIVITY) % 512; // 512为循环周期（2圈）
    } else {
        remaining_delta_x += dir * ENCODER_BASE_SENSITIVITY;
    }
}

void encoder_y_callback(EC11_Direction dir, void* user_data) {
    if (current_mode == MODE_GAMEPAD) {
        gamepad_y = (gamepad_y + dir * GAMEPAD_SENSITIVITY) % 512;
    } else {
        remaining_delta_y += dir * ENCODER_BASE_SENSITIVITY;
    }
}

void hid_task(void);
static void send_keyboard_report(uint32_t btn);

SystemMode load_system_mode(void);
void save_system_mode(SystemMode mode);


//--------------------------------------------------------------------+
// 主程序
//---------------------------------------------------------------------
int main(void)
{
    // 硬件初始化
    board_init();
    tud_init(BOARD_TUD_RHPORT);
    current_mode = load_system_mode();

    // 按钮GPIO初始化
    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }

    sleep_ms(10); // 消抖
    bool mode_changed = false;
    SystemMode new_mode = current_mode;

    if (!gpio_get(BTN_START)) {
        reset_usb_boot(0, 0); // 进入Bootloader不保存模式
    } else {
        if (!gpio_get(BTN_BTA)) {
            new_mode = MODE_KEYBOARD_MOUSE;
            mode_changed = true;
        } else if (!gpio_get(BTN_BTB)) {
            new_mode = MODE_GAMEPAD;
            mode_changed = true;
        }

        if (mode_changed && new_mode != current_mode) {
            current_mode = new_mode;
            save_system_mode(current_mode); // 保存新模式
        }
    }

    // 编码器初始化
    ec11_init(&encoder_x, ENCODER_X_PIN_A, ENCODER_X_PIN_B, encoder_x_callback, NULL);
    ec11_init(&encoder_y, ENCODER_Y_PIN_A, ENCODER_Y_PIN_B, encoder_y_callback, NULL);

    while (1) 
    {
        tud_task();  // 处理USB事件
        ec11_update(&encoder_x);
        ec11_update(&encoder_y);
        hid_task();  // 处理HID报告
    }
}

//--------------------------------------------------------------------+
// HID功能实现
//---------------------------------------------------------------------
static uint32_t read_buttons(void)
{
    uint32_t btn_mask = 0;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (!gpio_get(button_pins[i])) {
            btn_mask |= (1 << i);
        }
    }
    return btn_mask;
}

static void send_keyboard_report(uint32_t btn)
{
    if (!tud_hid_ready()) return;

    static uint32_t prev_btn_state = 0;
    if (btn == prev_btn_state) return;
    prev_btn_state = btn;

    uint8_t keycode[6] = {0};
    uint8_t key_cnt = 0;

    for (int i = 0; i < BUTTON_COUNT && key_cnt < 6; i++) {
        if (btn & (1 << i)) {
            keycode[key_cnt++] = KEYMAP_MOUSE_MODE[i];
        }
    }
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, key_cnt ? keycode : NULL);
}


static void handle_keyboard_mouse_mode(uint32_t btn_state) {
    // 发送键盘报告
    send_keyboard_report(btn_state);

    // 处理鼠标移动
    int8_t step_x = 0;
    int8_t step_y = 0;

    // X轴处理
    if (fabsf(remaining_delta_x) >= 1.0f) {
        const float ideal_step = remaining_delta_x / SMOOTHING_FACTOR;
        int8_t quantized_step = (int8_t)roundf(ideal_step);
        
        if (fabsf(quantized_step) > fabsf(remaining_delta_x)) {
            quantized_step = (remaining_delta_x > 0) ? 1 : -1;
        }
        
        remaining_delta_x -= quantized_step;
        step_x = quantized_step * MOUSE_SENSITIVITY_MULTIPLIER;
    }

    // Y轴处理
    if (fabsf(remaining_delta_y) >= 1.0f) {
        const float ideal_step = remaining_delta_y / SMOOTHING_FACTOR;
        int8_t quantized_step = (int8_t)roundf(ideal_step);
        
        if (fabsf(quantized_step) > fabsf(remaining_delta_y)) {
            quantized_step = (remaining_delta_y > 0) ? 1 : -1;
        }
        
        remaining_delta_y -= quantized_step;
        step_y = quantized_step * MOUSE_SENSITIVITY_MULTIPLIER;
    }

    // 发送鼠标报告
    if ((step_x != 0 || step_y != 0) && tud_hid_ready()) {
        tud_hid_mouse_report(REPORT_ID_MOUSE, 0, step_x, step_y, 0, 0);
    }
}

static void handle_gamepad_mode(uint32_t btn_state) {
    // 映射按钮到游戏手柄
    uint32_t gamepad_buttons = 0;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (btn_state & (1 << i)) {
            gamepad_buttons |= (1 << KEYMAP_GAMEPAD_MODE[i]);
        }
    }

    // 处理编码器移动（循环绕转）
    int16_t mapped_x = (int16_t)gamepad_x * 255 / 256 - 255;
    int16_t mapped_y = (int16_t)gamepad_y * 255 / 256 - 255;

    // 转换为8位值（自动溢出实现循环）
    int8_t axis_x = (int8_t)(mapped_x);
    int8_t axis_y = (int8_t)(mapped_y);


    // 根据TinyUSB的API调整参数顺序
    if (tud_hid_ready()) {
        tud_hid_gamepad_report(REPORT_ID_GAMEPAD, 
                             axis_x,        // X
                             axis_y,        // Y
                             0,             // Z
                             0,             // Rz
                             0,             // Rx
                             0,             // Ry
                             0,             // Hat
                             gamepad_buttons);
    }
}

void hid_task(void)
{
    static uint32_t start_ms = 0;
    const uint32_t current_time = board_millis();

    if (current_time - start_ms < MAIN_LOOP_INTERVAL_MS) return;
    start_ms = current_time;

    const uint32_t btn_state = read_buttons();

    if (tud_suspended()) {
        if (btn_state) tud_remote_wakeup();
        return;
    }

    switch (current_mode) {
        case MODE_KEYBOARD_MOUSE:
            handle_keyboard_mouse_mode(btn_state);
            break;
        case MODE_GAMEPAD:
            handle_gamepad_mode(btn_state);
            break;
        default: break;
    }
} 

//--------------------------------------------------------------------+
// Flash存储操作
//---------------------------------------------------------------------
SystemMode load_system_mode(void) {
    const SystemConfig *config = (const SystemConfig *)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (config->magic == FLASH_CONFIG_MAGIC) {
        return config->mode;
    }
    return MODE_KEYBOARD_MOUSE; // 默认模式
}

void save_system_mode(SystemMode mode) {
    SystemConfig config = {
        .magic = FLASH_CONFIG_MAGIC,
        .mode = mode,
    };

    uint8_t sector[FLASH_SECTOR_SIZE] = {0};
    memcpy(sector, &config, sizeof(config));

    // 安全执行Flash操作
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, sector, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

//--------------------------------------------------------------------+
// USB HID回调函数
//---------------------------------------------------------------------
void tud_mount_cb(void) {}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
void tud_resume_cb(void) {}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                          hid_report_type_t report_type, uint8_t const* buffer,
                          uint16_t bufsize)
{
    (void)instance;
    if (report_type == HID_REPORT_TYPE_OUTPUT && 
        report_id == REPORT_ID_KEYBOARD &&
        bufsize > 0) 
    {
        // 处理LED状态
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                              hid_report_type_t report_type, uint8_t* buffer,
                              uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;
}