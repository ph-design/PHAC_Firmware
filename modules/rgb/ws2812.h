#ifndef WS2812_H
#define WS2812_H

#include "hardware/pio.h"

#define IS_RGBW false
#define NUM_PIXELS 150

void pattern_random(PIO pio, uint sm, uint len, uint t, float brightness);

typedef void (*pattern)(PIO pio, uint sm, uint len, uint t);

extern const struct {
    pattern pat;
    const char *name;
} pattern_table[];

uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void put_pixel(PIO pio, uint sm, uint32_t pixel_grb);

#endif // WS2812_H
