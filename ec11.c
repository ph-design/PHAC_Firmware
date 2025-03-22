#include "ec11.h"
#include "hardware/gpio.h"

// 编码器状态表 (前两bit为旧状态，后两bit为新状态)
static const int8_t transition_table[] = {
    [0b0000] = 0,  [0b0001] = -1, [0b0010] = 1,  [0b0011] = 0,
    [0b0100] = 1,  [0b0101] = 0,  [0b0110] = 0,  [0b0111] = -1,
    [0b1000] = -1, [0b1001] = 0,  [0b1010] = 0,  [0b1011] = 1,
    [0b1100] = 0,  [0b1101] = 1,  [0b1110] = -1, [0b1111] = 0
};

void ec11_init(EC11_Encoder* encoder, uint8_t pin_a, uint8_t pin_b, 
              EC11_Callback cb, void* user_data) {
    encoder->pin_a = pin_a;
    encoder->pin_b = pin_b;
    encoder->callback = cb;
    encoder->user_data = user_data;

    gpio_init(pin_a);
    gpio_set_dir(pin_a, GPIO_IN);
    gpio_pull_up(pin_a);

    gpio_init(pin_b);
    gpio_set_dir(pin_b, GPIO_IN);
    gpio_pull_up(pin_b);

    encoder->state = (gpio_get(pin_a) << 1) | gpio_get(pin_b);
}

void ec11_update(EC11_Encoder* encoder) {
    uint8_t new_state = (gpio_get(encoder->pin_a) << 1) | gpio_get(encoder->pin_b);
    uint8_t transition = (encoder->state << 2) | new_state;
    
    int8_t result = transition_table[transition];
    if(result != 0 && encoder->callback) {
        encoder->callback((EC11_Direction)result, encoder->user_data);
    }
    
    encoder->state = new_state;
}