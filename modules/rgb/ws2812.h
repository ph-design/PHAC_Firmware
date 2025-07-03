#pragma once

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

typedef struct
{
    PIO pio;
    uint sm;
    uint offset;
    uint pin;
    uint num_pixels;
} ws2812_t;

// 初始化WS2812灯带
bool ws2812_init(ws2812_t *strip, PIO pio, uint sm, uint pin, uint num_pixels, float freq);

// 设置单个像素颜色 (RGB)
void ws2812_set_pixel(ws2812_t *strip, uint32_t pixel_idx, uint8_t r, uint8_t g, uint8_t b);

// 更新所有像素到灯带
void ws2812_show(ws2812_t *strip);

// 清理资源
void ws2812_release(ws2812_t *strip);

// 预定义动画模式
void ws2812_pattern_snakes(ws2812_t *strip, uint t);
void ws2812_pattern_random(ws2812_t *strip);
void ws2812_pattern_sparkle(ws2812_t *strip);
void ws2812_pattern_greys(ws2812_t *strip, uint t);