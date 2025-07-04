// ws2812.c
#include "ws2812.h"
#include <stdlib.h>

// 静态全局状态
static PIO pio;
static uint sm;
static uint offset;

// 预定义模式表
const Pattern pattern_table[] = {
    {pattern_snakes, "Snakes!"},
    {pattern_random, "Random data"},
    {pattern_sparkle, "Sparkles"},
    {pattern_greys, "Greys"},
};
const uint pattern_count = sizeof(pattern_table) / sizeof(pattern_table[0]);

// 初始化WS2812
void ws2812_init()
{
// 验证引脚兼容性
#if WS2812_PIN >= NUM_BANK0_GPIOS
#error WS2812_PIN >= 32 not supported
#endif

    // 分配PIO资源
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
    hard_assert(success);

    // 初始化PIO程序（800kHz，RGB模式）
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);
}

// 清理资源
void ws2812_cleanup()
{
    pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
}

// 写入单个像素
void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

// RGB转32位颜色值
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// === 预定义模式函数 ===
void pattern_snakes(uint len, uint t)
{
    for (uint i = 0; i < len; ++i)
    {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

void pattern_random(uint len, uint t)
{
    if (t % 8)
        return;
    for (uint i = 0; i < len; ++i)
        put_pixel(rand());
}

void pattern_sparkle(uint len, uint t)
{
    if (t % 8)
        return;
    for (uint i = 0; i < len; ++i)
        put_pixel(rand() % 16 ? 0 : 0xffffff);
}

void pattern_greys(uint len, uint t)
{
    uint max = 100; // 亮度限制
    t %= max;
    for (uint i = 0; i < len; ++i)
    {
        put_pixel(t * 0x10101);
        if (++t >= max)
            t = 0;
    }
}