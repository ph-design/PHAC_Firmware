// modules/encoder/ec11.h
#ifndef EC11_H
#define EC11_H

#include "pico/stdlib.h"
#include "hardware/pio.h"

// 编码器旋转方向枚举
typedef enum {
    EC11_DIR_NONE = 0,
    EC11_DIR_CW = 1,    // 顺时针
    EC11_DIR_CCW = -1   // 逆时针
} EC11_Direction;

// 编码器回调函数类型
typedef void (*EC11_Callback)(EC11_Direction dir, void *user_data);

typedef struct EC11_Encoder
{
    // 原有字段
    uint pin_a;
    uint pin_b;
    PIO pio;
    uint sm;
    EC11_Callback callback;
    void *user_data;
    int32_t count;
    int32_t last_count;
    EC11_Direction last_direction;

    // 新增状态指针
    void *state_ptr;
} EC11_Encoder;

// 初始化EC11编码器
void ec11_init(EC11_Encoder *encoder, uint pin_a, uint pin_b, EC11_Callback callback, void *user_data);

// 更新EC11编码器状态
void ec11_update(EC11_Encoder *encoder);

// 获取EC11编码器当前计数
int32_t ec11_get_count(EC11_Encoder *encoder);

// 重置EC11编码器计数
void ec11_reset_count(EC11_Encoder *encoder, int32_t value);

#endif // EC11_H
