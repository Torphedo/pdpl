#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CONSOLE_HIDE = 0,
    CONSOLE_SHOW = 1,
    CONSOLE_TOGGLE = 2
}console_show_state;

bool console_setup(int16_t min_height);
void console_set_shown(console_show_state action);
bool console_redirect_stdio();
void console_set_min_height(int16_t min_height);

