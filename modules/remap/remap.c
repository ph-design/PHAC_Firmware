
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 
 #include "bsp/board_api.h"
 #include "tusb.h"

 enum  {
   BLINK_NOT_MOUNTED = 250,
   BLINK_MOUNTED = 1000,
   BLINK_SUSPENDED = 2500,
 };
 
 static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
 
 void led_blinking_task(void);
 
 /*------------- MAIN -------------*/
 int main(void)
 {
   board_init();
 
   // init device stack on configured roothub port
   tusb_rhport_init_t dev_init = {
     .role = TUSB_ROLE_DEVICE,
     .speed = TUSB_SPEED_AUTO
   };
   tusb_init(BOARD_TUD_RHPORT, &dev_init);
 
   if (board_init_after_tusb) {
     board_init_after_tusb();
   }
 
   while (1)
   {
     tud_task(); // tinyusb device task
     led_blinking_task();
   }
 }
 
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
 

 void tud_suspend_cb(bool remote_wakeup_en)
 {
   (void) remote_wakeup_en;
   blink_interval_ms = BLINK_SUSPENDED;
 }
 
 // Invoked when usb bus is resumed
 void tud_resume_cb(void)
 {
   blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
 }

 uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
 {
   // TODO not Implemented
   (void) itf;
   (void) report_id;
   (void) report_type;
   (void) buffer;
   (void) reqlen;
   blink_interval_ms = 10; // suspend LED blinking
 
   return 0;
 }

 void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    (void) itf;
    (void) report_id;
    (void) report_type;

    // 创建64字节响应缓冲区
    uint8_t response[64] = {0};
    const char prefix[] = "received:";
    const char suffix[] = " greeting from pico!";
    const uint8_t prefix_len = sizeof(prefix) - 1;
    const uint8_t suffix_len = sizeof(suffix) - 1;
    const uint16_t max_data_len = sizeof(response) - prefix_len - suffix_len;
    const uint16_t data_len = bufsize > max_data_len ? max_data_len : bufsize;

    // 解析接收数据
    char cmd[64] = {0};
    memcpy(cmd, buffer, bufsize < 63 ? bufsize : 63); // 保留最后一个字节为\0

    // 检测频率设置指令（格式："freq:500"）
    if (strncmp(cmd, "freq:", 5) == 0) {
        int freq = atoi(cmd + 5);
        if (freq >= 100 && freq <= 5000) { // 限制合理范围
            blink_interval_ms = freq;
            snprintf((char*)response, sizeof(response), "Frequency set to %dms", freq);
            tud_hid_report(0, response, sizeof(response));
            return;
        }
    }

    // 常规回显处理
    memcpy(response, prefix, prefix_len);
    memcpy(response + prefix_len, buffer, data_len);
    memcpy(response + prefix_len + data_len, suffix, suffix_len);
    tud_hid_report(0, response, sizeof(response));
}

 void led_blinking_task(void)
 {
   static uint32_t start_ms = 0;
   static bool led_state = false;
 
   // Blink every interval ms
   if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
   start_ms += blink_interval_ms;
 
   board_led_write(led_state);
   led_state = 1 - led_state; // toggle
 }