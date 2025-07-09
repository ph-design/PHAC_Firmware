// modules/encoder/ec11.c
#include "ec11.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ec11.pio.h"

// 初始化EC11编码器
void ec11_init(EC11_Encoder *encoder, uint pin_a, uint pin_b, EC11_Callback callback, void *user_data) {
    encoder->pin_a = pin_a;
    encoder->pin_b = pin_b;
    encoder->callback = callback;
    encoder->user_data = user_data;
    encoder->count = 0;
    encoder->last_count = 0;

    // 选择一个可用的PIO实例和状态机
    encoder->pio = pio0;
    encoder->sm = pio_claim_unused_sm(encoder->pio, true);

    // 加载PIO程序
    uint offset = pio_add_program(encoder->pio, &quadrature_encoder_program);
    
    // 初始化PIO程序
    quadrature_encoder_program_init(encoder->pio, encoder->sm, encoder->pin_a,3, true, 3);
}

// 更新EC11编码器状态
void ec11_update(EC11_Encoder *encoder) {
    // 获取当前计数
    encoder->count = quadrature_encoder_get_count(encoder->pio, encoder->sm);
    
    // 计算变化量
    int32_t delta = encoder->count - encoder->last_count;
    
    // 如果有变化且设置了回调函数
    if (delta != 0 && encoder->callback != NULL) {
        EC11_Direction dir = (delta > 0) ? EC11_DIR_CW : EC11_DIR_CCW;
        encoder->callback(dir, encoder->user_data);
    }
    
    // 更新上次计数值
    encoder->last_count = encoder->count;
}

// 获取EC11编码器当前计数
int32_t ec11_get_count(EC11_Encoder *encoder) {
    return encoder->count;
}

// 重置EC11编码器计数
void ec11_reset_count(EC11_Encoder *encoder, int32_t value) {
    encoder->count = value;
    encoder->last_count = value;
    // 注意：这里我们不重置PIO计数器，因为我们只跟踪相对变化
}
