#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

// 配置覆盖机制
#ifndef NUM_PIXELS
#define NUM_PIXELS 150
#endif

#ifndef WS2812_PIN
#define WS2812_PIN 11
#endif

#ifndef UPDATE_INTERVAL_MS
#define UPDATE_INTERVAL_MS 10
#endif

// DMA状态
typedef enum
{
    DMA_IDLE,
    DMA_TRANSFERRING,
    DMA_COMPLETE
} dma_state_t;

// 初始化/清理函数
void ws2812_init(void);
void ws2812_cleanup(void);

// 像素操作
void put_pixel(uint32_t pixel_grb);
void put_pixel_at(uint index, uint32_t pixel_grb);
void clear_pixels(void);

// 颜色转换
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);

// 帧缓冲区访问
uint32_t *ws2812_get_buffer(void);
void ws2812_update_buffer(void);

// DMA控制
void ws2812_start_transfer(void);
bool ws2812_is_busy(void);
dma_state_t ws2812_get_dma_state(void);

// 预定义模式
void pattern_rainbow(uint t);
void pattern_black(uint t);


#endif