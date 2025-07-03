#include "ws2812.h"
#include <stdlib.h>

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

bool ws2812_init(ws2812_t *strip, PIO pio, uint sm, uint pin, uint num_pixels, float freq) {
    strip->pio = pio;
    strip->sm = sm;
    strip->pin = pin;
    strip->num_pixels = num_pixels;
    
    // 检查GPIO有效性
    if (pin >= NUM_BANK0_GPIOS)
        return false;

    // 加载PIO程序并初始化 (固定为RGB模式)
    strip->offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, strip->offset, pin, freq, false); // 最后一个参数固定为false

    return true;
}

void ws2812_set_pixel(ws2812_t *strip, uint32_t pixel_idx, uint8_t r, uint8_t g, uint8_t b) {
    if (pixel_idx >= strip->num_pixels)
        return;
    uint32_t color = urgb_u32(r, g, b);
    pio_sm_put_blocking(strip->pio, strip->sm, color << 8u);
}

void ws2812_show(ws2812_t *strip) {
    // 发送复位信号
    sleep_us(50);
}

void ws2812_release(ws2812_t *strip) {
    pio_sm_set_enabled(strip->pio, strip->sm, false);
    pio_remove_program(strip->pio, &ws2812_program, strip->offset);
}

void ws2812_pattern_snakes(ws2812_t *strip, uint t) {
    for (uint i = 0; i < strip->num_pixels; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            ws2812_set_pixel(strip, i, 0xff, 0, 0);
        else if (x >= 15 && x < 25)
            ws2812_set_pixel(strip, i, 0, 0xff, 0);
        else if (x >= 30 && x < 40)
            ws2812_set_pixel(strip, i, 0, 0, 0xff);
        else
            ws2812_set_pixel(strip, i, 0, 0, 0);
    }
    ws2812_show(strip);
}

void ws2812_pattern_random(ws2812_t *strip) {
    for (uint i = 0; i < strip->num_pixels; ++i) {
        uint8_t r = rand() % 256;
        uint8_t g = rand() % 256;
        uint8_t b = rand() % 256;
        ws2812_set_pixel(strip, i, r, g, b);
    }
    ws2812_show(strip);
}

void ws2812_pattern_sparkle(ws2812_t *strip) {
    for (uint i = 0; i < strip->num_pixels; ++i) {
        if (rand() % 16 == 0)
            ws2812_set_pixel(strip, i, 0xff, 0xff, 0xff);
        else
            ws2812_set_pixel(strip, i, 0, 0, 0);
    }
    ws2812_show(strip);
}

void ws2812_pattern_greys(ws2812_t *strip, uint t) {
    uint max = 100;
    t %= max;
    for (uint i = 0; i < strip->num_pixels; ++i) {
        uint8_t val = t;
        ws2812_set_pixel(strip, i, val, val, val);
        if (++t >= max) t = 0;
    }
    ws2812_show(strip);
}