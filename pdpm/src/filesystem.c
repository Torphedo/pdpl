#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB

#include <Windows.h>

#include <physfs.h>

#include "filesystem.h"
#include "path.h"

static bool enumerate_again = true;

static const char vfs_msg[] = "[\033[32mVirtual Filesystem\033[0m]";
static const char vfs_err[] = "[\033[31mVirtual Filesystem\033[0m]";

static PHYSFS_EnumerateCallbackResult recursive_mount(void *data, const char *origdir, const char *fname) {
    static bool restart_from_beginning = false;
    if (strcmp(origdir, "/") == 0) {
        restart_from_beginning = false;
    }

    if (restart_from_beginning) {
        return PHYSFS_ENUM_STOP;
    }

    char virtual_path[MAX_PATH] = {0 };
    sprintf(virtual_path, "%s/%s", origdir, fname);

    if (PHYSFS_isDirectory(virtual_path)) {
        PHYSFS_enumerate(virtual_path, recursive_mount, NULL);
        return PHYSFS_ENUM_OK;
    }

    if (path_has_extension(fname, ".7z") || path_has_extension(fname, ".zip")) {
        // Get the full real virtual_path of archive, to mount and check mount point.
        const char* real_path = PHYSFS_getRealDir(virtual_path);

        // If the real directory is a zip, then someone has nested zips.
        if (path_has_extension(real_path, ".7z") || path_has_extension(real_path, ".zip")) {
            printf("%s: Nested zip files are not supported, at least not right now. Take the nested zip file out of %s.\n", vfs_err, real_path);
            return PHYSFS_ENUM_OK;
        }
        sprintf(virtual_path, "%s%s%s", real_path, PHYSFS_getDirSeparator(), fname);

        PHYSFS_getMountPoint(virtual_path);
        if (PHYSFS_getLastErrorCode() == PHYSFS_ERR_NOT_MOUNTED) {
            PHYSFS_mount(virtual_path, origdir, true);
            PHYSFS_setErrorCode(PHYSFS_ERR_OK);
            enumerate_again = true;
            printf("%s: Mounted %s at %s\n", vfs_msg, virtual_path, origdir);
            restart_from_beginning = true;
            return PHYSFS_ENUM_STOP;
        }
    }
    return PHYSFS_ENUM_OK;
}

void vfs_setup() {
    printf("\n%s: Starting up...\n", vfs_msg);
    PHYSFS_init(NULL);

    // Get game RoamingState path
    char app_path[MAX_PATH] = { 0 };
    get_ms_esper_path(app_path);

    // Enabling writing to this directory and make the mod / plugin folders if necessary
    PHYSFS_setWriteDir(app_path);
    PHYSFS_mkdir("mods/plugins");

    // Add mod folder to the search path
    strcat(app_path, "mods");
    if (PHYSFS_mount(app_path, "/Assets/Data/", true) == 0) {
        printf("%s: Failed to add %s to the virtual filesystem. (%s)\n", vfs_err, app_path, PHYSFS_getLastError());
    }
    else {
        printf("%s: Mounted %s at /Assets/Data/\n", vfs_msg, app_path);
    }

    while(enumerate_again) {
        enumerate_again = false;
        PHYSFS_enumerate("/", recursive_mount, NULL);
    }
    printf("%s: Finished setting up.\n\n", vfs_msg);
}
