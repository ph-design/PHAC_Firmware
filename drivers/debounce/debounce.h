#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdint.h>
#include "hardware/gpio.h"

#define DEBOUNCE_TIME_MS 5  // 消抖时间5ms

typedef struct {
    uint8_t pin;            // GPIO引脚
    uint8_t stable_state;   // 稳定状态
    uint8_t last_state;     // 上次状态
    uint32_t last_time;     // 上次变化时间
} DebounceButton;

void debounce_init(DebounceButton* buttons, const uint8_t* pins, uint8_t count);
void debounce_update(DebounceButton* buttons, uint8_t count);
uint32_t debounce_get_states(DebounceButton* buttons, uint8_t count);

#endif