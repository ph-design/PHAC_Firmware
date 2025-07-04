#include "ws2812.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

// 颜色格式转换函数
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// 初始化函数
bool ws2812_init(ws2812_t *strip, PIO pio, uint sm, uint pin, uint num_pixels, float freq)
{
    // 参数检查
    if (pin >= NUM_BANK0_GPIOS)
        return false;

    // 设置灯带参数
    strip->pio = pio;
    strip->sm = sm;
    strip->pin = pin;
    strip->num_pixels = num_pixels;

    // 加载PIO程序并初始化
    strip->offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, strip->offset, pin, freq, false);

    return true;
}

// 设置像素颜色
void ws2812_set_pixel(ws2812_t *strip, uint32_t pixel_idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (pixel_idx >= strip->num_pixels)
        return;
    uint32_t color = urgb_u32(r, g, b);
    pio_sm_put_blocking(strip->pio, strip->sm, color << 8u);
}

// 刷新显示
void ws2812_show(ws2812_t *strip)
{
    sleep_us(50); // 发送复位信号
}

// 资源释放
void ws2812_release(ws2812_t *strip)
{
    pio_sm_set_enabled(strip->pio, strip->sm, false);
    pio_remove_program(strip->pio, &ws2812_program, strip->offset);
}