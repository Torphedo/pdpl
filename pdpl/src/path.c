#include <stdint.h>
#include <string.h>

#include <shlobj.h>

#include "path.h"

void get_ms_esper_path(char* string) {
    static char ms_esper_path[MAX_PATH] = {0};
    static uint32_t path_length = 0;

    // Only get the path if it's empty
    if (ms_esper_path[0] == 0) {
        if (SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, 0, ms_esper_path) == S_OK) {
            path_length = strlen(ms_esper_path);
            // Remove the "AC" from the end of the path
            memset(&ms_esper_path[path_length - 2], 0, 2);
            // Add "RoamingState\" to the end
            strncat(ms_esper_path, "RoamingState\\", sizeof("RoamingState\\"));
            path_length += sizeof("RoamingState\\") - 1;
        }
    }
    strncpy(string, ms_esper_path, path_length);
}
