#include "ws2812.h"
#include <stdlib.h>
#include <string.h>



static PIO pio;
static uint sm;
static uint offset;
static uint32_t pixel_buffer[NUM_PIXELS];
static uint32_t dma_buffer[NUM_PIXELS];
static int dma_chan;
static dma_state_t dma_state = DMA_IDLE;
static uint32_t last_update_time = 0;
static float current_brightness = 1.0f;

void __isr dma_complete_handler(void)
{
    if (dma_channel_get_irq0_status(dma_chan))
    {
        dma_channel_acknowledge_irq0(dma_chan);
        dma_state = DMA_COMPLETE;
    }
}

void ws2812_init(void)
{

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);
    hard_assert(success);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);

    clear_pixels();

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan,
        &c,
        &pio->txf[sm],
        dma_buffer,
        NUM_PIXELS,
        false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void ws2812_cleanup(void)
{
    dma_channel_unclaim(dma_chan);
    pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
}

uint32_t *ws2812_get_buffer(void)
{
    return pixel_buffer;
}

void ws2812_update_buffer(void)
{
    memcpy(dma_buffer, pixel_buffer, NUM_PIXELS * sizeof(uint32_t));
}

void ws2812_start_transfer(void)
{
    if (dma_state != DMA_IDLE)
        return;

    while (!pio_sm_is_tx_fifo_empty(pio, sm))
    {
        tight_loop_contents();
    }

    dma_channel_set_read_addr(dma_chan, dma_buffer, true);
    dma_state = DMA_TRANSFERRING;
}

bool ws2812_is_busy(void)
{
    return dma_state != DMA_IDLE;
}

dma_state_t ws2812_get_dma_state(void)
{
    return dma_state;
}

void ws2812_update_state(void)
{
    if (dma_state == DMA_COMPLETE)
    {
        dma_state = DMA_IDLE;
    }
}

static uint32_t adjusted_rgb_to_grb(uint8_t r, uint8_t g, uint8_t b)
{
    // 应用亮度调整
    r = (uint8_t)(r * current_brightness);
    g = (uint8_t)(g * current_brightness);
    b = (uint8_t)(b * current_brightness);

    return (((uint32_t)g << 16) | ((uint32_t)r << 8) | b) << 8;
}

void set_button_color(uint index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < NUM_PIXELS)
    {
        pixel_buffer[index] = adjusted_rgb_to_grb(r, g, b);
    }
}

void ws2812_set_brightness(float brightness)
{
    current_brightness = (brightness < 0.0f) ? 0.0f : (brightness > 1.0f) ? 1.0f
                                                                          : brightness;
}

void clear_pixels(void)
{
    memset(pixel_buffer, 0, NUM_PIXELS * sizeof(uint32_t));
}



void pattern_rainbow(uint t)
{
    for (uint i = 0; i < NUM_PIXELS; ++i)
    {
        uint8_t r, g, b;
        uint16_t hue = (t + i * 256 / NUM_PIXELS) % 256;

        if (hue < 85)
        {
            r = 255 - hue * 3;
            g = hue * 3;
            b = 0;
        }
        else if (hue < 170)
        {
            hue -= 85;
            r = 0;
            g = 255 - hue * 3;
            b = hue * 3;
        }
        else
        {
            hue -= 170;
            r = hue * 3;
            g = 0;
            b = 255 - hue * 3;
        }

        set_button_color(i, r, g, b);
    }
}

void pattern_black(uint t)
{
    // 全黑模式
    for (uint i = 0; i < NUM_PIXELS; ++i)
    {
        set_button_color(i, 0,0,0);
    }
}