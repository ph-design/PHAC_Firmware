#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "hardware/gpio.h"
#include "EC11.h"
#include "math.h"

//--------------------------------------------------------------------+
// 硬件配置宏定义
//--------------------------------------------------------------------+

// 按钮配置
#define BUTTON_COUNT           7
#define BUTTON_0_PIN           0
#define BUTTON_1_PIN           1
#define BUTTON_2_PIN           2
#define BUTTON_3_PIN           3
#define BUTTON_4_PIN           4
#define BUTTON_5_PIN           5
#define BUTTON_6_PIN           6
static const uint8_t button_pins[BUTTON_COUNT] = {BUTTON_5_PIN,

                            BUTTON_0_PIN,   BUTTON_1_PIN,   BUTTON_2_PIN,   BUTTON_3_PIN,
                            
                                        BUTTON_4_PIN,    BUTTON_6_PIN
};

// 键码映射
#define KEYMAP_A               HID_KEY_A
#define KEYMAP_B               HID_KEY_B
#define KEYMAP_C               HID_KEY_C
#define KEYMAP_D               HID_KEY_D
#define KEYMAP_E               HID_KEY_E
#define KEYMAP_F               HID_KEY_F
#define KEYMAP_G               HID_KEY_G
static const uint8_t keymap[BUTTON_COUNT] = {
    KEYMAP_A, KEYMAP_B, KEYMAP_C, KEYMAP_D,
    KEYMAP_E, KEYMAP_F, KEYMAP_G
};

// 编码器配置
#define ENCODER_X_PIN_A        7
#define ENCODER_X_PIN_B        8
#define ENCODER_Y_PIN_A        9
#define ENCODER_Y_PIN_B        10

// 鼠标参数
#define ENCODER_BASE_SENSITIVITY 5       // 编码器基础灵敏度
#define MOUSE_SENSITIVITY_MULTIPLIER 2   // 鼠标移动倍率
#define SMOOTHING_FACTOR       5.0f      // 平滑系数（值越大越平滑）
#define HID_REPORT_INTERVAL_MS 1         // HID报告间隔(ms)

//--------------------------------------------------------------------+
// 全局变量
//--------------------------------------------------------------------+
EC11_Encoder encoder_x; // X轴编码器
EC11_Encoder encoder_y; // Y轴编码器

static int8_t mouse_x = 0;
static int8_t mouse_y = 0;
static uint32_t prev_btn_state = 0;
static float remaining_delta_x = 0.0f;
static float remaining_delta_y = 0.0f;
static uint32_t last_mouse_report_time = 0;

void hid_task(void);

//--------------------------------------------------------------------+
// 编码器回调函数
//--------------------------------------------------------------------+

void encoder_x_callback(EC11_Direction dir, void* user_data) {
    remaining_delta_x += dir * ENCODER_BASE_SENSITIVITY;
}

void encoder_y_callback(EC11_Direction dir, void* user_data) {
    remaining_delta_y += dir * ENCODER_BASE_SENSITIVITY;
}

//--------------------------------------------------------------------+
// 主程序
//--------------------------------------------------------------------+

int main(void)
{
    // 硬件初始化
    board_init();
    tud_init(BOARD_TUD_RHPORT);

    // 按钮GPIO初始化
    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }

    // 编码器初始化
    ec11_init(&encoder_x, ENCODER_X_PIN_A, ENCODER_X_PIN_B, encoder_x_callback, NULL);
    ec11_init(&encoder_y, ENCODER_Y_PIN_A, ENCODER_Y_PIN_B, encoder_y_callback, NULL);

    while (1) 
    {
        tud_task();  // 处理USB事件
        ec11_update(&encoder_x);
        ec11_update(&encoder_y);
        hid_task();
    }
}

//--------------------------------------------------------------------+
// HID功能实现
//--------------------------------------------------------------------+

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

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
    if (!tud_hid_ready()) return;

    if (btn == prev_btn_state) return;
    prev_btn_state = btn;

    uint8_t keycode[6] = {0};
    uint8_t key_cnt = 0;

    for (int i = 0; i < BUTTON_COUNT && key_cnt < 6; i++) {
        if (btn & (1 << i)) {
            keycode[key_cnt++] = keymap[i];
        }
    }

    tud_hid_keyboard_report(report_id, 0, key_cnt ? keycode : NULL);
}

void hid_task(void)
{
    static uint32_t start_ms = 0;
    const uint32_t current_time = board_millis();

    if (current_time - start_ms < HID_REPORT_INTERVAL_MS) return;
    start_ms = current_time;

    // 读取按键状态
    const uint32_t btn_state = read_buttons();

    // 远程唤醒处理
    if (tud_suspended()) {
        if (btn_state) tud_remote_wakeup();
        return;
    }

    // 发送键盘报告
    send_hid_report(REPORT_ID_KEYBOARD, btn_state);

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

//--------------------------------------------------------------------+
// USB HID回调函数（保持不变）
//--------------------------------------------------------------------+

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