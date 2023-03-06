#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <shlobj.h>

#include "path.h"

void get_ms_esper_path(char* string) {
    if (SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, 0, string) == S_OK) {
        uint16_t length = strlen(string); // Remove the "AC" folder name from the end of the path.
        string[length - 1] = 0;
        string[length - 2] = 0;
        strcat(string, "RoamingState\\");
    }
}

bool path_is_plugin_folder(const char* path) {
    uint16_t pos = strlen(path) - 2; // Skip over trailing slash

    pos = path_pos_next_folder(path, pos);
    bool plugins = (strncmp(&path[pos + 2], "plugins", sizeof("plugins") - 1) == 0);

    pos = path_pos_next_folder(path, pos);
    bool mods = (strncmp(&path[pos + 2], "mods", sizeof("mods") - 1) == 0);

    return plugins && mods;
}

bool path_has_extension(const char* path, const char* extension) {
    uint32_t pos = strlen(path);
    uint16_t ext_length = strlen(extension);

    // File extension is longer than input string.
    if (ext_length > pos) {
        return false;
    }
    return (strncmp(&path[pos - ext_length], extension, ext_length) == 0);
}

void path_fix_backslashes(char* path) {
    uint16_t pos = strlen(path) - 1; // Subtract 1 so that we don't need to check null terminator
    while (pos > 0) {
        if (path[pos] == '\\') {
            path[pos] = '/';
        }
        pos--;
    }
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

void path_truncate(char* path, uint16_t pos) {
    path[--pos] = 0; // Removes last character to take care of trailing "\\" or "/".
    while(path[pos] != '\\' && path[pos] != '/') {
        path[pos--] = 0;
    }
}

void path_make_physfs_friendly(char* path) {
    // Copy string to a new buffer
    char string_cpy[MAX_PATH] = {0};
    strncpy(string_cpy, path, MAX_PATH);
    memset(path, 0, MAX_PATH); // Delete input string

    // Make all directory separators into '/'
    path_fix_backslashes(string_cpy);
    for(uint16_t i = 0; i < MAX_PATH; i++) {
        // saveoptions.ini is in the UWP RoamingState folder, it's easier to just search for it and replace it entirely.
        if (memcmp(&string_cpy[strlen(string_cpy) - sizeof("saveoptions.ini") + 1], "saveoptions.ini", sizeof("saveoptions.ini")) == 0) {
            sprintf(path, "/Assets/Data/saveoptions.ini");
            break;
        }
        if (string_cpy[i] == '/') {
            // In case of "//" in the filepath, skip first slash.
            if (string_cpy[i + 1] == '/') {
                continue;
            }
            else if (memcmp(&string_cpy[i], "/../", 4) == 0) {
                path_truncate(path, i); // Remove the last directory that was added to the output string
                i += 2; // Skip over the "/.."
                continue;
            }
            else if (memcmp(&string_cpy[i], "/./", 3) == 0) {
                i++; // Skip over the "/."
                continue;
            }
        }
        // Append string_cpy[i] to the output path.
        strncat(path, &string_cpy[i], 1);
    }
}
