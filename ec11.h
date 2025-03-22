#ifndef EC11_H
#define EC11_H

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EC11_CW = 1,    // 顺时针旋转
    EC11_CCW = -1    // 逆时针旋转
} EC11_Direction;

typedef void (*EC11_Callback)(EC11_Direction dir, void* user_data);

typedef struct {
    uint8_t pin_a;
    uint8_t pin_b;
    uint8_t state;
    EC11_Callback callback;
    void* user_data;
} EC11_Encoder;

void ec11_init(EC11_Encoder* encoder, uint8_t pin_a, uint8_t pin_b, EC11_Callback cb, void* user_data);
void ec11_update(EC11_Encoder* encoder);

#ifdef __cplusplus
}
#endif

#endif