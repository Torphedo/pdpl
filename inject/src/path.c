#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include <shlobj.h>

#include "path.h"

// We assume the input string has a length of MAX_PATH.
void get_ms_esper_path(char* string) {
    if (SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, 0, string) == S_OK) {
        uint32_t path_length = strlen(string);
        // Remove the "AC" from the end of the path
        memset(&string[path_length - 2], 0, 2);
        // Add "RoamingState\" to the end
        strncat(string, "RoamingState\\", sizeof("RoamingState\\"));
    }
}

bool file_exists(const char* path) {
    struct stat buffer = {0};
    return (stat(path, &buffer) == EXIT_SUCCESS);
}
