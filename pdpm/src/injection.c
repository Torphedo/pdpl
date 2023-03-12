#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB

#include <Windows.h>

#include <physfs.h>

#include "path.h"
#include "injection.h"

static const char injector_msg[] = "[\033[32mPlugin Injector\033[0m]";
static const char injector_err[] = "[\033[31mPlugin Injector\033[0m]";

void inject_plugins() {
    static const char* dir = "/plugins/";
    char** file_list = PHYSFS_enumerateFiles(dir);

    for (char** i = file_list; *i != NULL; i++) {
        if (path_has_extension(*i, ".dll")) {
            char full_path[MAX_PATH] = {0};

            // Get full virtual filesystem path.
            sprintf(full_path, "%s%s", dir, *i);
            const char* real_path = PHYSFS_getRealDir(full_path);

            if (path_has_extension(real_path, ".zip") || path_has_extension(real_path, ".7z")) {
                printf("%s: Plugins in zip archives are currently unsupported. Skipping %s...\n", injector_err, *i);
                continue;
            }

            // Real search path + / or \ + filename
            sprintf(full_path, "%s\\plugins\\%s", real_path, *i);

            printf("%s: Injecting %s.\n", injector_msg, *i);
            LoadLibraryA(full_path);
        }
    }
    PHYSFS_freeList(file_list);
}
