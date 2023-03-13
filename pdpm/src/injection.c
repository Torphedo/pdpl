#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB

#include <Windows.h>

#include <physfs.h>
#include <MemoryModule.h>

#include "path.h"
#include "injection.h"

static const char injector_msg[] = "[\033[32mPlugin Loader\033[0m]";

void manual_load_library(uint8_t* module_data) {

}

void inject_plugins() {
    static const char* dir = "/plugins/";
    char** file_list = PHYSFS_enumerateFiles(dir);

    for (char** i = file_list; *i != NULL; i++) {
        if (path_has_extension(*i, ".dll")) {
            char full_path[MAX_PATH] = {0};

            // Get full virtual filesystem path.
            sprintf(full_path, "%s%s", dir, *i);

            PHYSFS_File* dll_file = PHYSFS_openRead(full_path);
            if (dll_file != NULL) {
                int64_t filesize = PHYSFS_fileLength(dll_file);
                uint8_t* plugin_data = calloc(1, filesize);
                if (plugin_data != NULL) {
                    PHYSFS_readBytes(dll_file, plugin_data, filesize);
                    MemoryLoadLibrary(plugin_data, filesize);
                }
            }

            printf("%s: Injecting %s.\n", injector_msg, *i);
        }
    }
    PHYSFS_freeList(file_list);
}
