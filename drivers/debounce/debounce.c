#include "debounce.h"
#include "bsp/board_api.h"

void debounce_init(DebounceButton* buttons, const uint8_t* pins, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        buttons[i].pin = pins[i];
        buttons[i].stable_state = gpio_get(pins[i]);
        buttons[i].last_state = buttons[i].stable_state;
        buttons[i].last_time = board_millis();
        
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
}

void debounce_update(DebounceButton* buttons, uint8_t count) {
    const uint32_t now = board_millis();
    
    for (uint8_t i = 0; i < count; i++) {
        DebounceButton* btn = &buttons[i];
        const uint8_t current_state = gpio_get(btn->pin);
        
        if (current_state != btn->last_state) {
            btn->last_state = current_state;
            btn->last_time = now;
        }
        
        if ((now - btn->last_time) > DEBOUNCE_TIME_MS) {
            btn->stable_state = current_state;
        }
    }
}

uint32_t debounce_get_states(DebounceButton* buttons, uint8_t count) {
    uint32_t states = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (!buttons[i].stable_state) {  // 低电平有效
            states |= (1 << i);
        }
    }
    return states;
}