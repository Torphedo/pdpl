#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CONSOLE_HIDE = 0,
    CONSOLE_SHOW = 1,
    CONSOLE_TOGGLE = 2
}console_show_state;

typedef enum {
    DEST_PROCESS_CONSOLE,
    DEST_RESET
}console_destination;

typedef enum {
    CONSOLE_CREATE,
    CONSOLE_ATTACH // This attaches to any existing console, only plugins after the initial manager should use this.
}console_create;

bool console_setup(int16_t min_height, console_create method);
void console_set_shown(console_show_state action);
bool console_redirect_stdio(console_destination destination);
void console_set_min_height(int16_t min_height);

