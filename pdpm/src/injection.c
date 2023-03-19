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
static const char injector_warn[] = "[\033[33mPlugin Loader\033[0m]";

// This casting is really annoying, but it will just throw errors about the slightly different types if I don't.
const plugin_api api = {
        .plugin_cleanup    = MemoryFreeLibrary,
        .PHYSFS_exists     = (bool (*)(const char*)) PHYSFS_exists,
        .PHYSFS_openRead   = (void*(*)(const char*)) PHYSFS_openRead,
        .PHYSFS_openWrite  = (void*(*)(const char*)) PHYSFS_openWrite,
        .PHYSFS_openAppend = (void*(*)(const char*)) PHYSFS_openAppend,
        .PHYSFS_readBytes  = (int64_t (*)(void*, void*, uint64_t)) PHYSFS_readBytes,
        .PHYSFS_writeBytes = (int64_t (*)(void*, const void*, uint64_t)) PHYSFS_writeBytes,
        .PHYSFS_fileLength = (int64_t (*)(void*)) PHYSFS_fileLength,
        .PHYSFS_tell       = (int64_t (*)(void*)) PHYSFS_tell,
        .PHYSFS_seek       = (int32_t (*)(void*, uint64_t)) PHYSFS_seek,
        .PHYSFS_close      = (int (*)(void*)) PHYSFS_close
};

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
                    HMEMORYMODULE plugin_handle = MemoryLoadLibrary(plugin_data, filesize);

                    PLUGINMAIN plugin_main = (PLUGINMAIN) MemoryGetProcAddress(plugin_handle, "plugin_main");
                    FARPROC dll_main = MemoryGetProcAddress(plugin_handle, "DllMain");
                    if (dll_main != NULL) {
                        if (plugin_main != NULL) {
                            printf("%s: %s exports DllMain() and plugin_main(), both will be executed.\n", injector_warn, *i);
                        }
                        else {
                            // TODO: This is kind of a long message, maybe shorten it?
                            printf("%s: %s doesn't export plugin_main(). Using plugin_main() is preferred, because it gives access to the virtual filesystem and allows you to unload your plugin properly.\n", injector_warn, *i);
                        }
                    }
                    if (plugin_main != NULL) {
                        plugin_main(plugin_handle, &api);
                    }
                }
            }

            printf("%s: Loaded %s.\n", injector_msg, *i);
        }
    }
    PHYSFS_freeList(file_list);
}
