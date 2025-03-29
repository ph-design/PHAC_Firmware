#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "ec11.h"
#include "debounce.h"

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
#define BTN_BTA 6
#define BTN_BTB 5
#define BTN_BTC 4
#define BTN_BTD 3
#define BTN_FXL 2
#define BTN_START 1
#define BTN_FXR 0

// 模式选择引脚（原BTA/BTB/BTC）
#define MODE_PIN_1 BTN_BTA
#define MODE_PIN_2 BTN_BTB
#define MODE_PIN_3 BTN_BTC
#define MAIN_LOOP_INTERVAL_US 250 // 250μs = 4kHz
#define SAMPLING_INTERVAL_US 250  // 4kHz采样率
#define DEBOUNCE_DELAY_MS 5       // 消抖延时设为5ms

typedef struct
{
    uint8_t encoder_id;
    uint8_t cw_key;  // 顺时针按键
    uint8_t ccw_key; // 逆时针按键
} EncoderConfig;

typedef struct
{
    uint32_t next_toggle; // 下一次状态切换时间
    uint16_t interval;    // 闪烁间隔(ms)
    uint8_t remain_times; // 剩余切换次数
    bool led_state;       // 当前LED状态
} LED_Blink_State;


static EC11_Encoder encoder1, encoder2;
static EncoderConfig encoder1_cfg, encoder2_cfg;
static int16_t joystick_x = 0;
static int16_t joystick_y = 0;
static debounce_t btn_debounce[7];
static volatile bool current_btn_states[7] = {false};
static volatile bool btn_changed = false;
static LED_Blink_State led_blink = {0};
static uint32_t last_hid_report = 0;

//--------------------------------------------------------------------+
// 全局变量
//--------------------------------------------------------------------+
OperationMode current_mode = MODE1;
bool boot_mode_done = false;
uint32_t boot_start_time = 0;
bool prev_report_btn_state[7] = {false};
int16_t prev_joystick_x = 0, prev_joystick_y = 0;

//--------------------------------------------------------------------+
// 函数声明
//--------------------------------------------------------------------+
OperationMode detect_boot_mode(void);
void blink_led_pattern_nonblock(uint8_t count, uint16_t interval_ms);
void update_led_state(void);
void send_mode_hid_report(void);
void reinit_mode_pins(void);
bool timer_callback(repeating_timer_t *rt);

//--------------------------------------------------------------------+
// 编码器回调
//---------------------------------------------------------------------
static void encoder_handler(EC11_Direction dir, void *user_data)
{
    if (!tud_hid_ready())
        return;
    EncoderConfig *cfg = (EncoderConfig *)user_data;

    switch (current_mode)
    {
    case MODE1: // 键鼠模式
    {
        int8_t delta = (dir == EC11_CW) ? 5 : -5;
        if (cfg->encoder_id == 0)
        { // 左编码器控制X轴
            tud_hid_mouse_report(REPORT_ID_MOUSE, 0, delta, 0, 0, 0);
        }
        else
        { // 右编码器控制Y轴
            tud_hid_mouse_report(REPORT_ID_MOUSE, 0, 0, delta, 0, 0);
        }
    }
    break;

    case MODE2: // 手柄模式
    {
        int16_t delta = (dir == EC11_CW) ? 512 : -512;
        if (cfg->encoder_id == 0)
        { // 左编码器控制X轴
            joystick_x = (joystick_x + delta > 32767) ? 32767 : (joystick_x + delta < -32768) ? -32768
                                                                                              : joystick_x + delta;
        }
        else
        { // 右编码器控制Y轴
            joystick_y = (joystick_y + delta > 32767) ? 32767 : (joystick_y + delta < -32768) ? -32768
                                                                                              : joystick_y + delta;
        }
    }
    break;

    default: // 其他模式保持原有功能
        if (cfg->cw_key && cfg->ccw_key)
        {
            uint8_t key = (dir == EC11_CW) ? cfg->cw_key : cfg->ccw_key;
            uint8_t keycode[6] = {key};
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, (uint8_t[6]){0});
        }
    }
}

const uint btn_pins[] = {BTN_BTA, BTN_BTB, BTN_BTC, BTN_BTD, BTN_FXL, BTN_START, BTN_FXR};

//--------------------------------------------------------------------+
// 主程序
//--------------------------------------------------------------------+
int main(void)
{
    board_init();
    stdio_init_all();

    // 初始化按钮GPIO
    for (int i = 0; i < 7; i++)
    {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
        debounce_init(&btn_debounce[i], btn_pins[i]);
    }

    // 检测START按钮进入刷写模式
    if (!gpio_get(BTN_START))
    {
        reset_usb_boot(0, 0);
    }

    // 设置4kHz采样定时器
    repeating_timer_t timer;
    add_repeating_timer_us(-SAMPLING_INTERVAL_US, timer_callback, NULL, &timer);

    // 检测启动模式
    current_mode = detect_boot_mode();
    reinit_mode_pins();
    boot_mode_done = true;
    boot_start_time = board_millis();
    printf("System Started in Mode: %d\n", current_mode);

    // LED初始化
    if (current_mode != MODE1)
        blink_led_pattern_nonblock(3, 200);
    else
        board_led_write(true);

    // 初始化编码器
    encoder1_cfg.encoder_id = 0;
    ec11_init(&encoder1, 7, 8, encoder_handler, &encoder1_cfg);
    encoder2_cfg.encoder_id = 1;
    ec11_init(&encoder2, 9, 10, encoder_handler, &encoder2_cfg);

    tud_init(BOARD_TUD_RHPORT);

    while (1)
    {
        uint32_t start_time = time_us_32();

        tud_task();
        ec11_update(&encoder1);
        ec11_update(&encoder2);

        // 处理按钮状态变化
        if (btn_changed)
        {
            btn_changed = false;
            bool need_report = false;

            for (int i = 0; i < 7; i++)
            {
                if (current_btn_states[i] != prev_report_btn_state[i])
                {
                    prev_report_btn_state[i] = current_btn_states[i];
                    need_report = true;
                    if (current_btn_states[i])
                        blink_led_pattern_nonblock(1, 100);
                }
            }

            if (need_report && (board_millis() - last_hid_report >= 1)) // 最小1ms间隔
            {
                send_mode_hid_report();
                last_hid_report = board_millis();
            }
        }

        // 处理摇杆报告
        if (current_mode == MODE2 &&
            (joystick_x != prev_joystick_x || joystick_y != prev_joystick_y))
        {
            prev_joystick_x = joystick_x;
            prev_joystick_y = joystick_y;
            send_mode_hid_report();
        }

        update_led_state();

        // 精确循环间隔控制
        while ((time_us_32() - start_time) < MAIN_LOOP_INTERVAL_US)
        {
            tight_loop_contents();
        }
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
            return (OperationMode)(i + 1); // MODE1~3
    }
    return MODE1; // 默认模式
}

bool timer_callback(repeating_timer_t *rt) {
    for (int i = 0; i < 7; i++) {
        bool has_changed = debounce_read(&btn_debounce[i]);
        if (has_changed) {
            bool new_state = btn_debounce[i].stable_state == KEY_ACTIVE_STATE;
            if (new_state != current_btn_states[i]) {
                current_btn_states[i] = new_state;
                btn_changed = true;
            }
        }
    }
    return true;
}

//--------------------------------------------------------------------+
// HID报告发送
//--------------------------------------------------------------------+
static uint32_t counter = 0;
void send_mode_hid_report(void)
{
    counter++;
    if (!boot_mode_done || !tud_hid_ready())
        return;

    bool btn_states[7];
    memcpy(btn_states, prev_report_btn_state, sizeof(btn_states));

    switch (current_mode)
    {
    case MODE1: // 键鼠模式
    {
        uint8_t keycode[6] = {0};
        uint8_t idx = 0;
        if (btn_states[0])
            keycode[idx++] = HID_KEY_D; // BTA
        if (btn_states[1])
            keycode[idx++] = HID_KEY_F; // BTB
        if (btn_states[2])
            keycode[idx++] = HID_KEY_J; // BTC
        if (btn_states[3])
            keycode[idx++] = HID_KEY_K; // BTD
        if (btn_states[4])
            keycode[idx++] = HID_KEY_N; // FXL
        if (btn_states[5])
            keycode[idx++] = HID_KEY_Y; // START
        if (btn_states[6])
            keycode[idx++] = HID_KEY_M; // FXR
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    }
    break;

    case MODE2: // 手柄模式
    {
        hid_gamepad_report_t report = {0};
        // 按钮映射
        if (btn_states[0])
            report.buttons |= GAMEPAD_BUTTON_A; // BTA
        if (btn_states[1])
            report.buttons |= GAMEPAD_BUTTON_B; // BTB
        if (btn_states[2])
            report.buttons |= GAMEPAD_BUTTON_X; // BTC
        if (btn_states[3])
            report.buttons |= GAMEPAD_BUTTON_Y; // BTD
        if (btn_states[4])
            report.buttons |= GAMEPAD_BUTTON_TL; // FXL
        if (btn_states[5])
            report.buttons |= GAMEPAD_BUTTON_START; // START
        if (btn_states[6])
            report.buttons |= GAMEPAD_BUTTON_TR; // FXR

        // 摇杆数据
        report.x = joystick_x;
        report.y = joystick_y;

        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
    }
    break;

    case MODE3: // 手柄模式
    {
        hid_gamepad_report_t report = {0};
        if (btn_states[0])
            report.buttons |= GAMEPAD_BUTTON_A;
        if (btn_states[1])
            report.buttons |= GAMEPAD_BUTTON_B;
        if (btn_states[2])
            report.buttons |= GAMEPAD_BUTTON_X;
        if (btn_states[3])
            report.buttons |= GAMEPAD_BUTTON_Y;
        if (btn_states[4])
            report.buttons |= GAMEPAD_BUTTON_0;
        if (btn_states[5])
            report.buttons |= GAMEPAD_BUTTON_START;
        if (btn_states[6])
            report.buttons |= GAMEPAD_BUTTON_1;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
    }
    break;
    }
}

void scanrate_hid_report(void)
{
    static uint32_t last_print = 0;
    uint32_t now = board_millis();

    if (now - last_print >= 1000)
    {
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
void blink_led_pattern_nonblock(uint8_t count, uint16_t interval_ms)
{
    led_blink.interval = interval_ms;
    led_blink.remain_times = count * 2; // 每个周期包含亮灭
    led_blink.next_toggle = board_millis();
    led_blink.led_state = false; // 初始状态为灭
    board_led_write(false);      // 立即更新状态
}

void update_led_state(void)
{
    if (led_blink.remain_times > 0 &&
        (int32_t)(board_millis() - led_blink.next_toggle) >= 0)
    {

        led_blink.led_state = !led_blink.led_state;
        board_led_write(led_blink.led_state);

        led_blink.next_toggle += led_blink.interval;
        led_blink.remain_times--;

        if (led_blink.remain_times == 0)
        {
            board_led_write(true); // 结束保持常亮
        }
    }
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