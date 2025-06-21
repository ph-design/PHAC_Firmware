#ifndef EC11_H
#define EC11_H

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EC11_CW = 1,    // Clockwise rotation
    EC11_CCW = -1   // Counterclockwise rotation
} EC11_Direction;

typedef void (*EC11_Callback)(EC11_Direction dir, void* user_data);

typedef struct {
    uint8_t pin_a;
    uint8_t pin_b;
    EC11_Callback callback;
    void* user_data;
    
    uint8_t state;           // Current stable state
    uint8_t last_raw_state;  // Last read raw state
    uint32_t last_change_time; // Last state change time
    uint8_t stable_counter;  // State stability counter
} EC11_Encoder;

void ec11_init(EC11_Encoder* encoder, uint8_t pin_a, uint8_t pin_b, EC11_Callback cb, void* user_data);
void ec11_update(EC11_Encoder* encoder);

#ifdef __cplusplus
}
#endif

#endif