#include "ec11.h"
#include "hardware/gpio.h"
#include "pico/time.h"

// Encoder state transition table
static const int8_t transition_table[] = {
    [0b0000] = 0,  [0b0001] = -1, [0b0010] = 1,  [0b0011] = 0,
    [0b0100] = 1,  [0b0101] = 0,  [0b0110] = 0,  [0b0111] = -1,
    [0b1000] = -1, [0b1001] = 0,  [0b1010] = 0,  [0b1011] = 1,
    [0b1100] = 0,  [0b1101] = 1,  [0b1110] = -1, [0b1111] = 0
};

// Add state history buffer

void ec11_init(EC11_Encoder* encoder, uint8_t pin_a, uint8_t pin_b, 
              EC11_Callback cb, void* user_data) {
    encoder->pin_a = pin_a;
    encoder->pin_b = pin_b;
    encoder->callback = cb;
    encoder->user_data = user_data;
    encoder->stable_counter = 0;

    gpio_init(pin_a);
    gpio_set_dir(pin_a, GPIO_IN);
    gpio_pull_up(pin_a);

    gpio_init(pin_b);
    gpio_set_dir(pin_b, GPIO_IN);
    gpio_pull_up(pin_b);

    // Initialize state from current pin values
    uint8_t init_state = (gpio_get(pin_a) << 1) | gpio_get(pin_b);
    encoder->state = init_state;
    encoder->last_raw_state = init_state;
    encoder->last_change_time = time_us_32();
}

void ec11_update(EC11_Encoder* encoder) {
    const uint32_t DEBOUNCE_TIME_US = 1000;      // Debounce time in microseconds
    const uint8_t STABLE_THRESHOLD = 5;          // Number of stable reads required
    uint32_t current_time = time_us_32();
    uint8_t new_state = (gpio_get(encoder->pin_a) << 1) | gpio_get(encoder->pin_b);
    
    // If state changes, reset debounce logic
    if (new_state != encoder->last_raw_state) {
        encoder->last_raw_state = new_state;
        encoder->last_change_time = current_time;
        encoder->stable_counter = 0;
        return;
    }
    
    // Check if the state has been stable for enough time
    if ((current_time - encoder->last_change_time) > DEBOUNCE_TIME_US) {
        if (encoder->stable_counter < STABLE_THRESHOLD) {
            encoder->stable_counter++;
        }
        
        // If state is stable and different from last recorded stable state
        if (encoder->stable_counter >= STABLE_THRESHOLD && 
            new_state != encoder->state) {
            
            // Combine previous and current state to form a transition code
            uint8_t transition = (encoder->state << 2) | new_state;
            int8_t result = transition_table[transition];
            
            // If a valid rotation is detected, call the callback
            if(result != 0 && encoder->callback) {
                encoder->callback((EC11_Direction)result, encoder->user_data);
            }
            
            // Update the stable state
            encoder->state = new_state;
        }
    }
}