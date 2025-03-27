#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "ec11.h"

//--------------------------------------------------------------------+
// 模式定义和GPIO配置
//--------------------------------------------------------------------+
typedef enum
{
    MODE_BOOTLOADER, // 特殊模式：进入刷写模式
    MODE1,           // 鼠标模式
    MODE2,           // 键盘模式
    MODE3            // 手柄模式
} OperationMode;

// 按钮GPIO定义
#define BTN_BTA    6
#define BTN_BTB    5
#define BTN_BTC    4
#define BTN_BTD    3
#define BTN_FXL    2
#define BTN_START  1
#define BTN_FXR    0

// 模式选择引脚（原BTA/BTB/BTC）
#define MODE_PIN_1 BTN_BTA
#define MODE_PIN_2 BTN_BTB
#define MODE_PIN_3 BTN_BTC

typedef struct {
    uint8_t cw_key;  // 顺时针按键
    uint8_t ccw_key; // 逆时针按键
} EncoderConfig;

static EC11_Encoder encoder1, encoder2;
static EncoderConfig encoder1_cfg, encoder2_cfg;

//--------------------------------------------------------------------+
// 全局变量
//--------------------------------------------------------------------+
OperationMode current_mode = MODE1;
bool boot_mode_done = false;
uint32_t boot_start_time = 0;
bool prev_btn_state[7] = {false}; // 7个按钮状态

//--------------------------------------------------------------------+
// 函数声明
//--------------------------------------------------------------------+
OperationMode detect_boot_mode(void);
void blink_led_pattern(uint8_t count);
void send_mode_hid_report(void);
void reinit_mode_pins(void);
void scanrate_hid_report(void);

//--------------------------------------------------------------------+
// 编码器回调
//---------------------------------------------------------------------
static void encoder_handler(EC11_Direction dir, void* user_data) {
    EncoderConfig* cfg = (EncoderConfig*)user_data;
    uint8_t key = (dir == EC11_CW) ? cfg->cw_key : cfg->ccw_key;
    
    if(key != 0) {
        uint8_t keycode[6] = {key};
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        sleep_ms(20);
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, (uint8_t[6]){0});
    }
}

//--------------------------------------------------------------------+
// 主程序
//--------------------------------------------------------------------+
int main(void)
{
    board_init();
    stdio_init_all();

    // 首先检测START按钮是否按下进入刷写模式
    gpio_init(BTN_START);
    gpio_set_dir(BTN_START, GPIO_IN);
    gpio_pull_up(BTN_START);
    sleep_ms(10); // 消抖
    if (!gpio_get(BTN_START)) {
        reset_usb_boot(0, 0); // 进入USB刷写模式
    }

    // 初始化所有按钮引脚
    const uint btn_pins[] = {BTN_BTA, BTN_BTB, BTN_BTC, BTN_BTD, BTN_FXL, BTN_START, BTN_FXR};
    for (int i = 0; i < 7; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
    }

    // 检测启动模式
    current_mode = detect_boot_mode();
    boot_start_time = board_millis();
    printf("System Started in Mode: %d\n", current_mode);

    // LED初始化
    if (current_mode != MODE1)
        blink_led_pattern(3);
    board_led_write(true);

    // 初始化编码器
    encoder1_cfg.cw_key = HID_KEY_VOLUME_UP;
    encoder1_cfg.ccw_key = HID_KEY_VOLUME_DOWN;
    ec11_init(&encoder1, 7, 8, encoder_handler, &encoder1_cfg);

    encoder2_cfg.cw_key = HID_KEY_PAGE_UP;
    encoder2_cfg.ccw_key = HID_KEY_PAGE_DOWN;
    ec11_init(&encoder2, 9, 10, encoder_handler, &encoder2_cfg);

    tud_init(BOARD_TUD_RHPORT);

    while (1)
    {
        tud_task();
        if (board_millis() - boot_start_time > 500 && !boot_mode_done)
        {
            reinit_mode_pins();
            boot_mode_done = true;
        }
        ec11_update(&encoder1);
        ec11_update(&encoder2);
        send_mode_hid_report();
        scanrate_hid_report();
    }
}

//--------------------------------------------------------------------+
// 模式检测函数
//--------------------------------------------------------------------+
OperationMode detect_boot_mode(void)
{
    bool mode_pins[] = {
        !gpio_get(MODE_PIN_1),
        !gpio_get(MODE_PIN_2),
        !gpio_get(MODE_PIN_3)
    };

    for (int i = 0; i < 3; i++) {
        if (mode_pins[i]) return (OperationMode)(i+1); // MODE1~3
    }
    return MODE1; // 默认模式
}

//--------------------------------------------------------------------+
// HID报告发送
//--------------------------------------------------------------------+
static uint32_t counter = 0;
void send_mode_hid_report(void)
{
    counter++;
    if (!boot_mode_done || !tud_hid_ready()) return;

    bool btn_states[] = {
        !gpio_get(BTN_BTA),
        !gpio_get(BTN_BTB),
        !gpio_get(BTN_BTC),
        !gpio_get(BTN_BTD),
        !gpio_get(BTN_FXL),
        !gpio_get(BTN_START),
        !gpio_get(BTN_FXR)
    };

    // 检测状态变化并闪烁LED
    for (int i = 0; i < 7; i++) {
        if (btn_states[i] && !prev_btn_state[i]) {
            blink_led_pattern(1);
        }
        prev_btn_state[i] = btn_states[i];
    }

    switch (current_mode)
    {
    case MODE1: // 鼠标模式
    {
        int8_t x = 0, y = 0;
        uint8_t buttons = 0;
        
        if (btn_states[0]) buttons |= MOUSE_BUTTON_LEFT;    // BTA
        if (btn_states[1]) buttons |= MOUSE_BUTTON_RIGHT;   // BTB
        if (btn_states[2]) buttons |= MOUSE_BUTTON_MIDDLE;  // BTC
        if (btn_states[3]) x = 5;                          // BTD
        if (btn_states[4]) y = -5;                          // FXL
        if (btn_states[6]) y = 5;                           // FXR
        
        tud_hid_mouse_report(REPORT_ID_MOUSE, buttons, x, y, 0, 0);
    }
    break;

    case MODE2: // 键盘模式
    {
        uint8_t keycode[6] = {0};
        uint8_t idx = 0;
        if (btn_states[0]) keycode[idx++] = HID_KEY_A;      // BTA
        if (btn_states[1]) keycode[idx++] = HID_KEY_B;      // BTB
        if (btn_states[2]) keycode[idx++] = HID_KEY_C;      // BTC
        if (btn_states[3]) keycode[idx++] = HID_KEY_D;      // BTD
        if (btn_states[4]) keycode[idx++] = HID_KEY_F1;     // FXL
        if (btn_states[5]) keycode[idx++] = HID_KEY_ENTER;  // START
        if (btn_states[6]) keycode[idx++] = HID_KEY_F2;     // FXR
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    }
    break;

    case MODE3: // 手柄模式
    {
        hid_gamepad_report_t report = {0};
        if (btn_states[0]) report.buttons |= GAMEPAD_BUTTON_A;
        if (btn_states[1]) report.buttons |= GAMEPAD_BUTTON_B;
        if (btn_states[2]) report.buttons |= GAMEPAD_BUTTON_X;
        if (btn_states[3]) report.buttons |= GAMEPAD_BUTTON_Y;
        if (btn_states[4]) report.buttons |= GAMEPAD_BUTTON_0;
        if (btn_states[5]) report.buttons |= GAMEPAD_BUTTON_START;
        if (btn_states[6]) report.buttons |= GAMEPAD_BUTTON_1;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
    }
    break;
    }
}


void scanrate_hid_report(void)
{
    static uint32_t last_print = 0;
    uint32_t now = board_millis();

    if (now - last_print >= 1000) {
        printf("Scan rate: %lu Hz\n", counter);
        counter = 0;
        last_print = now;
    }
}

//--------------------------------------------------------------------+
// 引脚重新初始化
//--------------------------------------------------------------------+
void reinit_mode_pins(void)
{
    for (int pin = MODE_PIN_1; pin <= MODE_PIN_3; pin++)
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
}

//--------------------------------------------------------------------+
// LED控制函数
//--------------------------------------------------------------------+
void blink_led_pattern(uint8_t count)
{
    for (uint8_t i = 0; i < count; i++)
    {
        board_led_write(true);
        sleep_ms(100);
        board_led_write(false);
        sleep_ms(100);
    }
    board_led_write(true);
}

//--------------------------------------------------------------------+
// USB回调函数
//--------------------------------------------------------------------+
void tud_mount_cb(void) {}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
void tud_resume_cb(void) {}
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) { return 0; }
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {}