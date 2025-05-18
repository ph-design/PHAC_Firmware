#include "debounce.h" // 添加对应的头文件

void debounce_init(debounce_t *key, uint gpio_pin) {
    // 初始化GPIO为输入模式（需添加）
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_IN);
    gpio_pull_up(gpio_pin);
    key->gpio_pin = gpio_pin;
    key->last_state = gpio_get(gpio_pin);
    key->stable_state = key->last_state;
    key->last_time = get_absolute_time();
}

bool debounce_read(debounce_t *key) {
    bool current_state = gpio_get(key->gpio_pin);
    absolute_time_t now = get_absolute_time();
    int64_t diff_us = absolute_time_diff_us(key->last_time, now);

    /* 检测到状态变化时更新时间戳 */
    if (current_state != key->last_state) {
        key->last_time = now;
        key->last_state = current_state;
        return false;
    }

    /* 稳定状态判断：当状态保持时间超过消抖延时 */
    if (diff_us >= (DEBOUNCE_DELAY_MS * 1000)) {
        /* 仅当稳定状态与之前记录的不同时返回 true */
        if (current_state != key->stable_state) {
            key->stable_state = current_state;
            return true; // 状态变化（按下或释放）
        }
    }
    return false;
}