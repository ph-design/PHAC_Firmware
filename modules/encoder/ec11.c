#include "ec11.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ec11.pio.h"
#include "pico/time.h"
#include <stdlib.h>
#include <math.h>

// 平滑参数配置
#define SMOOTHING_FACTOR 12  // 每个物理事件生成的逻辑事件数
#define EVENT_INTERVAL_MS 1 // 事件分发间隔(毫秒)
#define QUEUE_SIZE 64       // 事件队列大小(增大以支持更多插值点)
#define MAX_ENCODERS 2      // 支持的最大编码器数量

static inline float smootherstep(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

typedef struct
{
    EC11_Direction dir;
    uint32_t scheduled_time;
    uint8_t step_index;      // 当前是第几个插值点 (0 到 SMOOTHING_FACTOR-1)
    uint8_t total_steps;     // 总插值点数
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
static bool queue_push(EncoderState *state, EC11_Direction dir, uint8_t step_idx, uint8_t total)
{
    if (state->queue_count >= QUEUE_SIZE)
        return false;

    // 使用Smootherstep计算非均匀时间间隔
    float progress = (float)(step_idx + 1) / (float)total;
    float eased_progress = smootherstep(progress);
    
    // 基础时间 + 缓动后的延迟
    uint32_t base_delay_us = EVENT_INTERVAL_MS * 1000 * total;
    uint32_t eased_delay_us = (uint32_t)(base_delay_us * eased_progress);
    
    state->event_queue[state->queue_tail].dir = dir;
    state->event_queue[state->queue_tail].step_index = step_idx;
    state->event_queue[state->queue_tail].total_steps = total;
    state->event_queue[state->queue_tail].scheduled_time = time_us_32() + eased_delay_us;
    
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

        // 生成平滑事件 - 使用缓动插值
        uint8_t events_per_click = SMOOTHING_FACTOR;
        uint8_t total_events = abs(delta) * events_per_click;
        
        for (int i = 0; i < total_events; i++)
        {
            // 每个事件都带有插值进度信息
            uint8_t step_in_click = i % events_per_click;
            queue_push(state, current_dir, step_in_click, events_per_click);
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