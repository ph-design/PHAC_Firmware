#include "debounce.h"

#ifndef DEBOUNCE_TIME_US
#define DEBOUNCE_TIME_US 7000
#endif

static void debounce_none(DebounceState *state)
{
    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        bool gpio_state = !gpio_get(state->pins[i]);
        state->states[i].pressed = gpio_state;
    }
}

static void asym_eager_defer_pk(DebounceState *state)
{
    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        bool gpio_state = !gpio_get(state->pins[i]);

        KeyState *key = &state->states[i];
        uint64_t now = time_us_64();

        if (gpio_state != key->pressed)
        {
            if (!key->active)
            {
                key->active = true;
                key->timestamp = now;

                if (gpio_state)
                {
                    key->pressed = true;
                }
            }
            else if ((now - key->timestamp) >= DEBOUNCE_TIME_US)
            {
                key->active = false;

                if (!gpio_state)
                {
                    key->pressed = false;
                }
            }
        }
        else
        {
            key->active = false;
        }
    }
}

void debounce_init(DebounceState *state, const uint8_t *pins)
{
    state->pins = pins;
    state->mode = ASYM_EAGER_DEFER_PK;

    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        state->states[i] = (KeyState){
            .pressed = false,
            .active = false,
            .timestamp = 0};

        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
}

void debounce_update(DebounceState *state)
{
    switch (state->mode)
    {
    case DEBOUNCE_NONE:
        debounce_none(state);
        break;

    case ASYM_EAGER_DEFER_PK:
        asym_eager_defer_pk(state);
        break;
    }
}

uint32_t debounce_get_states(DebounceState *state)
{
    uint32_t states = 0;
    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        if (state->states[i].pressed)
        {
            states |= (1 << i);
        }
    }
    return states;
}

void debounce_set_mode(DebounceState *state, DebounceMode mode)
{
    state->mode = mode;
}