#include <physfs.h>
#include <string.h>

void atomic_log(const char* message, const char* file) {
    PHYSFS_File* physfs_handle = PHYSFS_openAppend(file);
    PHYSFS_writeBytes(physfs_handle, message, strlen(message));
    PHYSFS_writeBytes(physfs_handle, "\n", 1);
    PHYSFS_close(physfs_handle);
}
