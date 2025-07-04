#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"
#include "hardware/pio.h"

// WS2812灯带结构体
typedef struct
{
    PIO pio;
    uint sm;
    uint pin;
    uint num_pixels;
    uint offset;
} ws2812_t;

// 函数声明
bool ws2812_init(ws2812_t *strip, PIO pio, uint sm, uint pin, uint num_pixels, float freq);
void ws2812_set_pixel(ws2812_t *strip, uint32_t pixel_idx, uint8_t r, uint8_t g, uint8_t b);
void ws2812_show(ws2812_t *strip);
void ws2812_release(ws2812_t *strip);

#endif