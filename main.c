#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "ec11.h"

//--------------------------------------------------------------------+
// 模式定义和GPIO配置
//--------------------------------------------------------------------+
typedef enum
{
    MODE1, // 鼠标模式
    MODE2, // 键盘模式
    MODE3  // 手柄模式
} OperationMode;

typedef struct {
    uint8_t cw_key;  // 顺时针按键
    uint8_t ccw_key; // 逆时针按键
} EncoderConfig;

static EC11_Encoder encoder1, encoder2;
static EncoderConfig encoder1_cfg, encoder2_cfg;

static void encoder_handler(EC11_Direction dir, void* user_data) {
    EncoderConfig* cfg = (EncoderConfig*)user_data;
    uint8_t key = (dir == EC11_CW) ? cfg->cw_key : cfg->ccw_key;
    
    if(key != 0) {
        // 发送按键按下
        uint8_t keycode[6] = {key};
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        
        // 短延时后释放按键
        sleep_ms(20);
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, (uint8_t[6]){0});
    }
}

#define MODE_PIN_1 0
#define MODE_PIN_2 1
#define MODE_PIN_3 2

//--------------------------------------------------------------------+
// 全局变量
//--------------------------------------------------------------------+
OperationMode current_mode = MODE1;
bool boot_mode_done = false;
uint32_t boot_start_time = 0;
bool prev_btn_state[3] = {false};

//--------------------------------------------------------------------+
// 函数声明
//--------------------------------------------------------------------+
OperationMode detect_boot_mode(void);
void blink_led_pattern(uint8_t count);
void send_mode_hid_report(void);
void reinit_mode_pins(void);
void scanrate_hid_report(void);

//--------------------------------------------------------------------+
// 初始化代码
//--------------------------------------------------------------------+
int main(void)
{
    board_init();
    stdio_init_all();

    // 初始化模式选择引脚
    gpio_init(MODE_PIN_1);
    gpio_set_dir(MODE_PIN_1, GPIO_IN);
    gpio_pull_up(MODE_PIN_1);

    gpio_init(MODE_PIN_2);
    gpio_set_dir(MODE_PIN_2, GPIO_IN);
    gpio_pull_up(MODE_PIN_2);

    gpio_init(MODE_PIN_3);
    gpio_set_dir(MODE_PIN_3, GPIO_IN);
    gpio_pull_up(MODE_PIN_3);

    // 检测启动模式并记录时间
    current_mode = detect_boot_mode();
    boot_start_time = board_millis();
    printf("System Started in Mode: %d\n", current_mode);

    // LED初始化
    if (current_mode != MODE1)
        blink_led_pattern(3);
    board_led_write(true);

    // 初始化编码器1 (GPIO7和8)
    encoder1_cfg.cw_key = HID_KEY_VOLUME_UP;
    encoder1_cfg.ccw_key = HID_KEY_VOLUME_DOWN;
    ec11_init(&encoder1, 7, 8, encoder_handler, &encoder1_cfg);

    // 初始化编码器2（GPIO9和10）
    encoder2_cfg.cw_key = HID_KEY_PAGE_UP;
    encoder2_cfg.ccw_key = HID_KEY_PAGE_DOWN;
    ec11_init(&encoder2, 9, 10, encoder_handler, &encoder2_cfg);

    // 初始化USB
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
        scanrate_hid_report(); // 添加此行
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
        !gpio_get(MODE_PIN_3)};

    for (int i = 0; i < 3; i++)
    {
        if (mode_pins[i])
            return (OperationMode)i;
    }
    return MODE1;
}

//--------------------------------------------------------------------+
// HID报告发送函数
//--------------------------------------------------------------------+
static uint32_t counter = 0; // 扫描率计数器
void send_mode_hid_report(void)
{
    counter++;
    if (!boot_mode_done || !tud_hid_ready())
        return;

    bool curr_btn_state[] = {
        !gpio_get(MODE_PIN_1),
        !gpio_get(MODE_PIN_2),
        !gpio_get(MODE_PIN_3)};

    // 检测状态变化并闪烁LED
    for (int i = 0; i < 3; i++)
    {
        if (curr_btn_state[i] && !prev_btn_state[i])
        {
            blink_led_pattern(1);
        }
        prev_btn_state[i] = curr_btn_state[i];
    }

    switch (current_mode)
    {
    case MODE1: // 鼠标模式：左/右/移动
    {
        uint8_t buttons = 0;
        if (curr_btn_state[0])
            buttons |= MOUSE_BUTTON_LEFT;
        if (curr_btn_state[1])
            buttons |= MOUSE_BUTTON_RIGHT;
        if (curr_btn_state[2])
            tud_hid_mouse_report(REPORT_ID_MOUSE, buttons, 0,0, 5, 0);
        
    }
    break;

    case MODE2: // 键盘模式：A/B/C键
    {
        uint8_t keycode[6] = {0};
        uint8_t idx = 0;
        if (curr_btn_state[0])
            keycode[idx++] = HID_KEY_A;
        if (curr_btn_state[1])
            keycode[idx++] = HID_KEY_B;
        if (curr_btn_state[2])
            keycode[idx++] = HID_KEY_C;
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    }
    break;

    case MODE3: // 手柄模式：A/B/X按钮
    {
        hid_gamepad_report_t report = {0};
        if (curr_btn_state[0])
            report.buttons |= GAMEPAD_BUTTON_A;
        if (curr_btn_state[1])
            report.buttons |= GAMEPAD_BUTTON_B;
        if (curr_btn_state[2])
            report.buttons |= GAMEPAD_BUTTON_X;
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