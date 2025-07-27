#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

#ifndef NUM_PIXELS
#define NUM_PIXELS 150
#endif

#ifndef WS2812_PIN
#define WS2812_PIN 11
#endif

#ifndef UPDATE_INTERVAL_MS
#define UPDATE_INTERVAL_MS 10
#endif

typedef enum
{
    DMA_IDLE,
    DMA_TRANSFERRING,
    DMA_COMPLETE
} dma_state_t;

void ws2812_init(void);
void ws2812_cleanup(void);

void set_button_color(uint index, uint8_t r, uint8_t g, uint8_t b);
void ws2812_set_brightness(float brightness);
void clear_pixels(void);

uint32_t *ws2812_get_buffer(void);
void ws2812_update_buffer(void);

void ws2812_start_transfer(void);
bool ws2812_is_busy(void);
dma_state_t ws2812_get_dma_state(void);

void pattern_rainbow(uint t);
void pattern_black(uint t);


#endif