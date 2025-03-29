#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include "pico/stdlib.h"

#ifndef DEBOUNCE_DELAY_MS
#define DEBOUNCE_DELAY_MS 10  // 默认消抖延时10ms
#endif

#define KEY_ACTIVE_STATE 0     // 按键有效状态为低电平

typedef struct {
    uint gpio_pin;             
    bool last_state;           
    bool stable_state;         
    absolute_time_t last_time; 
} debounce_t;

void debounce_init(debounce_t *key, uint gpio_pin);
bool debounce_read(debounce_t *key);

#endif