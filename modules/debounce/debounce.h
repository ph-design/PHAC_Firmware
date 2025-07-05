#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/time.h"
#include "hardware/gpio.h"

#define BUTTON_COUNT 7

typedef enum
{
    DEBOUNCE_NONE,
    ASYM_EAGER_DEFER_PK
} DebounceMode;

typedef struct
{
    bool pressed;
    bool active;
    uint64_t timestamp;
} KeyState;

typedef struct
{
    const uint8_t *pins;
    KeyState states[BUTTON_COUNT];
    DebounceMode mode;
} DebounceState;

void debounce_init(DebounceState *state, const uint8_t *pins);
void debounce_update(DebounceState *state);
uint32_t debounce_get_states(DebounceState *state);
void debounce_set_mode(DebounceState *state, DebounceMode mode);

#endif