#include "ws2812.h"
#include <stdlib.h>
#include <string.h>

// 静态全局状态
static PIO pio;
static uint sm;
static uint offset;
static uint32_t pixel_buffer[NUM_PIXELS];
static uint32_t dma_buffer[NUM_PIXELS]; // DMA需要32位对齐
static int dma_chan;
static dma_state_t dma_state = DMA_IDLE;
static uint32_t last_update_time = 0;

// DMA完成中断处理
void __isr dma_complete_handler(void)
{
    if (dma_channel_get_irq0_status(dma_chan))
    {
        dma_channel_acknowledge_irq0(dma_chan);
        dma_state = DMA_COMPLETE;
    }
}

// 初始化WS2812
void ws2812_init(void)
{
// 验证引脚兼容性
#if WS2812_PIN >= NUM_BANK0_GPIOS
#error WS2812_PIN >= 32 not supported
#endif

    // 分配PIO资源
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
    hard_assert(success);

    // 初始化PIO程序
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);

    // 初始化缓冲区
    clear_pixels();

    // 配置DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan,
        &c,
        &pio->txf[sm], // 目标地址: PIO TX FIFO
        dma_buffer,    // 源地址: DMA缓冲区
        NUM_PIXELS,    // 传输次数
        false          // 不立即启动
    );

    // 设置DMA完成中断
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

// 清理资源
void ws2812_cleanup(void)
{
    dma_channel_unclaim(dma_chan);
    pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
}

// 获取帧缓冲区
uint32_t *ws2812_get_buffer(void)
{
    return pixel_buffer;
}

// 更新DMA缓冲区
void ws2812_update_buffer(void)
{
    // 复制像素数据到DMA缓冲区（需要32位对齐）
    memcpy(dma_buffer, pixel_buffer, NUM_PIXELS * sizeof(uint32_t));
}

// 启动DMA传输
void ws2812_start_transfer(void)
{
    if (dma_state != DMA_IDLE)
        return;

    // 确保PIO状态机准备好
    while (!pio_sm_is_tx_fifo_empty(pio, sm))
    {
        tight_loop_contents();
    }

    // 启动DMA传输
    dma_channel_set_read_addr(dma_chan, dma_buffer, true);
    dma_state = DMA_TRANSFERRING;
}

// 检查DMA状态
bool ws2812_is_busy(void)
{
    return dma_state != DMA_IDLE;
}

// 获取DMA状态
dma_state_t ws2812_get_dma_state(void)
{
    return dma_state;
}

// 处理DMA状态机
void ws2812_update_state(void)
{
    if (dma_state == DMA_COMPLETE)
    {
        // 传输完成，重置状态
        dma_state = DMA_IDLE;
    }
}

// 写入单个像素（直接操作帧缓冲区）
void put_pixel_at(uint index, uint32_t pixel_grb)
{
    if (index < NUM_PIXELS)
    {
        pixel_buffer[index] = pixel_grb;
    }
}

// 写入单个像素（追加模式）
void put_pixel(uint32_t pixel_grb)
{
    static uint index = 0;
    put_pixel_at(index, pixel_grb);
    index = (index + 1) % NUM_PIXELS;
}

// 清空所有像素
void clear_pixels(void)
{
    memset(pixel_buffer, 0, NUM_PIXELS * sizeof(uint32_t));
}

// RGB转32位颜色值
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// === 非阻塞式预定义模式函数 ===
void pattern_snakes(uint t)
{
    for (uint i = 0; i < NUM_PIXELS; ++i)
    {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel_at(i, urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel_at(i, urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel_at(i, urgb_u32(0, 0, 0xff));
        else
            put_pixel_at(i, 0);
    }
}

void pattern_random(uint t)
{
    if (t % 8)
        return;
    for (uint i = 0; i < NUM_PIXELS; ++i)
        put_pixel_at(i, rand() & 0x00ffffff); // 确保高位为0
}

void pattern_sparkle(uint t)
{
    if (t % 8)
        return;
    for (uint i = 0; i < NUM_PIXELS; ++i)
        put_pixel_at(i, (rand() % 16) ? 0 : 0xffffff);
}

void pattern_greys(uint t)
{
    uint max = 100; // 亮度限制
    t %= max;
    for (uint i = 0; i < NUM_PIXELS; ++i)
    {
        put_pixel_at(i, t * 0x10101);
        if (++t >= max)
            t = 0;
    }
}