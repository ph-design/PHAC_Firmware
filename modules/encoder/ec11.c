#include "ec11.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ec11.pio.h"
#include "pico/time.h"
#include <stdlib.h>
#include <math.h>

// 平滑参数配置
#define SMOOTHING_FACTOR 4  // 每个物理事件生成4个逻辑事件
#define EVENT_INTERVAL_MS 2 // 事件分发间隔(毫秒)
#define QUEUE_SIZE 32       // 事件队列大小
#define MAX_ENCODERS 2      // 支持的最大编码器数量

typedef struct
{
    EC11_Direction dir;
    uint32_t scheduled_time;
} EncoderEvent;

typedef struct
{
    EC11_Encoder *encoder;
    struct repeating_timer timer;
} EncoderTimerData;

// 为每个编码器维护独立的状态
typedef struct
{
    EncoderEvent event_queue[QUEUE_SIZE];
    uint8_t queue_head;
    uint8_t queue_tail;
    uint8_t queue_count;
    EncoderTimerData timer_data;
} EncoderState;

static EncoderState encoder_states[MAX_ENCODERS];
static uint8_t encoder_count = 0;

// 队列操作函数 (接收EncoderState指针)
static bool queue_push(EncoderState *state, EC11_Direction dir)
{
    if (state->queue_count >= QUEUE_SIZE)
        return false;

    state->event_queue[state->queue_tail].dir = dir;
    state->event_queue[state->queue_tail].scheduled_time =
        time_us_32() + (EVENT_INTERVAL_MS * 1000);
    state->queue_tail = (state->queue_tail + 1) % QUEUE_SIZE;
    state->queue_count++;
    return true;
}

static bool queue_pop(EncoderState *state, EncoderEvent *event)
{
    if (state->queue_count == 0)
        return false;

    *event = state->event_queue[state->queue_head];
    state->queue_head = (state->queue_head + 1) % QUEUE_SIZE;
    state->queue_count--;
    return true;
}

// 定时器回调函数 (接收EncoderState指针)
static bool encoder_timer_callback(struct repeating_timer *t)
{
    EncoderTimerData *timer_data = (EncoderTimerData *)t->user_data;
    EncoderState *state = (EncoderState *)timer_data->encoder->state_ptr;
    EncoderEvent event;
    uint32_t current_time = time_us_32();

    if (state->queue_count > 0 &&
        state->event_queue[state->queue_head].scheduled_time <= current_time)
    {
        if (queue_pop(state, &event))
        {
            EC11_Encoder *enc = timer_data->encoder;
            if (enc->callback)
            {
                enc->callback(event.dir, enc->user_data);
            }
        }
    }
    return true;
}

// 初始化EC11编码器
void ec11_init(EC11_Encoder *encoder, uint pin_a, uint pin_b,
               EC11_Callback callback, void *user_data)
{
    if (encoder_count >= MAX_ENCODERS)
        return;

    // 关联编码器与状态
    encoder->state_ptr = &encoder_states[encoder_count];
    encoder_count++;

    EncoderState *state = (EncoderState *)encoder->state_ptr;

    // 初始化状态
    state->queue_head = 0;
    state->queue_tail = 0;
    state->queue_count = 0;

    // 初始化编码器
    encoder->pin_a = pin_a;
    encoder->pin_b = pin_b;
    encoder->callback = callback;
    encoder->user_data = user_data;
    encoder->count = 0;
    encoder->last_count = 0;
    encoder->last_direction = EC11_DIR_NONE;

    encoder->pio = pio0;
    encoder->sm = pio_claim_unused_sm(encoder->pio, true);
    uint offset = pio_add_program(encoder->pio, &quadrature_encoder_program);
    quadrature_encoder_program_init(encoder->pio, encoder->sm, pin_a, 3, true, 3);

    // 设置定时器
    state->timer_data.encoder = encoder;
    add_repeating_timer_ms(-EVENT_INTERVAL_MS, encoder_timer_callback,
                           &state->timer_data, &state->timer_data.timer);
}

// 更新EC11编码器状态
void ec11_update(EC11_Encoder *encoder)
{
    EncoderState *state = (EncoderState *)encoder->state_ptr;

    encoder->count = quadrature_encoder_get_count(encoder->pio, encoder->sm);
    int32_t delta = encoder->count - encoder->last_count;

    if (delta != 0)
    {
        EC11_Direction current_dir = (delta > 0) ? EC11_DIR_CW : EC11_DIR_CCW;

        // 方向变化检测
        if (current_dir != encoder->last_direction)
        {
            state->queue_head = 0;
            state->queue_tail = 0;
            state->queue_count = 0;
            encoder->last_direction = current_dir;
        }

        // 生成平滑事件
        uint8_t events_to_add = abs(delta) * SMOOTHING_FACTOR;
        for (int i = 0; i < events_to_add; i++)
        {
            queue_push(state, current_dir);
        }

        encoder->last_count = encoder->count;
    }
}

// 获取EC11编码器当前计数
int32_t ec11_get_count(EC11_Encoder *encoder)
{
    return encoder->count;
}

// 重置EC11编码器计数
void ec11_reset_count(EC11_Encoder *encoder, int32_t value)
{
    encoder->count = value;
    encoder->last_count = value;
}