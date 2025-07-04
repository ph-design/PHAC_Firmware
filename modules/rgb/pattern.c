#include "pattern.h"
#include <stdlib.h>

// 随机颜色效果
void ws2812_pattern_random(ws2812_t *strip)
{
    for (uint i = 0; i < strip->num_pixels; ++i)
    {
        uint8_t r = rand() % 256;
        uint8_t g = rand() % 256;
        uint8_t b = rand() % 256;
        ws2812_set_pixel(strip, i, r, g, b);
    }
    ws2812_show(strip);
}