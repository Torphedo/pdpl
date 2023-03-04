#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "path.h"

bool path_is_plugin_folder(const char* path) {
    uint16_t pos = strlen(path) - 2; // Skip over trailing slash

    pos = path_pos_next_folder(path, pos);
    bool plugins = (strncmp(&path[pos + 2], "plugins", sizeof("plugins") - 1) == 0);

    pos = path_pos_next_folder(path, pos);
    bool mods = (strncmp(&path[pos + 2], "mods", sizeof("mods") - 1) == 0);

    return plugins && mods;
}

uint16_t path_pos_next_folder(const char* path, uint16_t start_pos) {
    uint16_t pos = start_pos;
    if (pos == 0) {
        return pos;
    }

    while(path[pos] != '\\' && path[pos] != '/') {
        pos--;
    }
    return pos - 1;
}

void path_truncate(char* path, uint16_t size) {
    path[size--] = 0; // Removes last character to take care of trailing "\\" or "/".
    while(path[size] != '\\' && path[size] != '/') {
        path[size--] = 0;
    }
}
