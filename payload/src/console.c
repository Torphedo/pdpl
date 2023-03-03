#include <stdio.h>

#include <Windows.h>
#include <processenv.h>

#include "console.h"

static bool console_shown = false;

void console_set_shown(console_show_state action) {
    if (action == CONSOLE_TOGGLE) {
        console_shown = !console_shown;
    }
    else {
        console_shown = action;
    }
    HWND window = FindWindowA("ConsoleWindowClass", NULL);
    ShowWindow(window, console_shown);
}

bool console_setup(int16_t min_height, console_create method) {
    bool success = false;

    // console_redirect_stdio(DEST_RESET);

    if (method == CONSOLE_CREATE) {
        success = AllocConsole();
    }
    else if (method == CONSOLE_ATTACH) {
        success = AttachConsole(GetCurrentProcessId());
    }

    if (success) {
        console_set_min_height(min_height);
        console_set_shown(CONSOLE_SHOW);
        return console_redirect_stdio(DEST_PROCESS_CONSOLE);
    }
    else {
        return false;
    }
}

void destroy_console()
{
    HWND window = FindWindowA("ConsoleWindowClass", NULL);
    ShowWindow(window, 0);
    FreeConsole();

}

bool console_redirect_stdio(console_destination destination) {
    bool result = true;
    FILE *fp;
    static char* dest_in = "CONIN$";
    static char* dest_out = "CONOUT$";
    if (destination == DEST_RESET)
    {
        dest_in = "NUL:";
        dest_out = "NUL:";
    }

    // Redirect STDIN if the console has an input handle
    if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE) {
        if (freopen_s(&fp, dest_in, "r", stdin) != 0) {
            result = false;
        } else {
            setvbuf(stdin, NULL, _IONBF, 0);
        }
    }
    // Redirect STDOUT if the console has an output handle
    if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE) {
        if (freopen_s(&fp, dest_out, "w", stdout) != 0) {
            result = false;
        } else {
            setvbuf(stdout, NULL, _IONBF, 0);
        }
    }
    // Redirect STDERR if the console has an error handle
    if (GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE) {
        if (freopen_s(&fp, dest_out, "w", stderr) != 0) {
            result = false;
        } else {
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }
    return result;
}

void console_set_min_height(int16_t min_height) {
    CONSOLE_SCREEN_BUFFER_INFO console_info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &console_info);
    if (console_info.dwSize.Y < min_height) {
        console_info.dwSize.Y = min_height;
    }
    SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), console_info.dwSize);
}
