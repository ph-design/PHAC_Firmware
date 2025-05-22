/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

// Interface index depends on the order in configuration descriptor
enum
{
  ITF_KEYBOARD = 0,
  ITF_MOUSE = 1,
  ITF_GENERIC = 2,
};

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum
{
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static bool led_state = false;
static char response_buf[64];

// HID Report Echo variables
static uint8_t received_data[64];
static uint8_t received_report_id = 0;
static uint8_t received_itf = 0;
static uint16_t received_size = 0;
static bool send_response = false;

void led_blinking_task(void);
void hid_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO};
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  if (board_init_after_tusb)
  {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();

    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void)remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if (board_millis() - start_ms < interval_ms)
    return; // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if (tud_suspended() && btn)
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }

  /*------------- Keyboard -------------*/
  if (tud_hid_n_ready(ITF_KEYBOARD))
  {
    // use to avoid send multiple consecutive zero report for keyboard
    static bool has_key = false;

    if (btn)
    {
      uint8_t keycode[6] = {0};
      keycode[0] = HID_KEY_A;

      tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, keycode);

      has_key = true;
    }
    else
    {
      // send empty key report if previously has key pressed
      if (has_key)
        tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, NULL);
      has_key = false;
    }
  }

  /*------------- Mouse -------------*/
  if (tud_hid_n_ready(ITF_MOUSE))
  {
    if (btn)
    {
      int8_t const delta = 5;

      // no button, right + down, no scroll pan
      tud_hid_n_mouse_report(ITF_MOUSE, 0, 0x00, delta, delta, 0, 0);
    }
  }
  /*------------- Generic HID -------------*/
  /*------------- Echo Generic HID Report -------------*/
  if (send_response)
  {
    if (tud_hid_n_ready(received_itf))
    {
      tud_hid_n_report(received_itf, received_report_id, received_data, received_size);
      send_response = false;
    }
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void)itf;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
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

/*if (itf == ITF_GENERIC)
{
  char cmd[65] = {0};
  memcpy(cmd, buffer, bufsize > 64 ? 64 : bufsize);

   解析频率指令
  if (strncmp(cmd, "freq:", 5) == 0)
  {
    int freq = atoi(cmd + 5);
    if (freq >= 100 && freq <= 5000)
    {
      blink_interval_ms = freq;
      // 构造响应，格式：原命令 + "received"
      uint8_t response_data[64];
      response_data[0] = 2; // Report ID = 1
      int data_len = snprintf((char *)response_data + 1, sizeof(response_data) - 1, "%s received", cmd);

      // 发送数据
      tud_hid_n_report(ITF_GENERIC, 1, response_data, data_len + 1);
    }
  }
}
}*/

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;

  if (board_millis() - start_ms < blink_interval_ms)
    return;
  start_ms = board_millis();

  board_led_write(led_state);
  led_state = !led_state;
}