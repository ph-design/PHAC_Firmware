// ws2812.h
#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

// 像素数量配置（可在主程序中覆盖）
#ifndef NUM_PIXELS
#define NUM_PIXELS 150
#endif

// 引脚配置（优先使用主程序定义）
#ifdef PICO_DEFAULT_WS2812_PIN
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN
#else
#define WS2812_PIN 11 // 默认GPIO11
#endif

// 帧率控制（毫秒，可在主程序中覆盖）
#ifndef UPDATE_INTERVAL_MS
#define UPDATE_INTERVAL_MS 10
#endif

// 颜色转换函数
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);

// 模式函数指针类型
typedef void (*pattern_func)(uint len, uint t);

// 初始化/清理函数
void ws2812_init();
void ws2812_cleanup();

// 像素操作
void put_pixel(uint32_t pixel_grb);

// 预定义模式函数
void pattern_snakes(uint len, uint t);
void pattern_random(uint len, uint t);
void pattern_sparkle(uint len, uint t);
void pattern_greys(uint len, uint t);

// 模式选择结构体
typedef struct
{
    pattern_func pat;
    const char *name;
} Pattern;

extern const Pattern pattern_table[];
extern const uint pattern_count;

#endif