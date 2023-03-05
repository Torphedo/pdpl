#include <stdio.h>

#include <MinHook.h>
#include <physfs.h>

#include "hooks.h"
#include "path.h"

typedef HANDLE (*CREATE_FILE_2)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);

CREATE_FILE_2 original_CreateFile2 = NULL;

bool hook_ReadFile(HANDLE hFile, LPVOID buffer, uint32_t bytes_to_read, LPDWORD bytes_read_ptr, LPOVERLAPPED overlapped) {
    printf("Intercepted call to ReadFile(), proceeding as normal.\n");
    return true;
    return ReadFile(hFile, buffer, bytes_to_read, bytes_read_ptr, overlapped);
}

HANDLE hook_CreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams) {
    char filename[MAX_PATH] = {0};
    PHYSFS_utf8FromUtf16(lpFileName, filename, MAX_PATH);
    path_fix_backslashes(filename);
    printf("CreateFile2(): Opened %s\n", filename);
    if (PHYSFS_exists(filename)) {
        printf("[File exists in modded filesystem]\n");
    }
    return original_CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
}

bool setup_hooks() {
    MH_Initialize();
    printf("Plugin Manager: Initializing hooks...\n");

    HMODULE kernelbase = GetModuleHandleW(L"KERNELBASE.dll");
    if (kernelbase == NULL) {
        printf("Plugin Manager: couldn't find KernelBase.dll.\n");
    }

    if (MH_CreateHookApi(L"KERNELBASE.dll", "CreateFile2", &hook_CreateFile2, (void**)&original_CreateFile2) != MH_OK) {
    // if (MH_CreateHook(&CreateFile2, hook_CreateFile2, (void**)&original_CreateFile2) != MH_OK) {
        printf("Plugin Manager: Failed to create hook from CreateFile2() to hook_CreateFile2().\n");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        printf("Failed to enable hook from CreateFile2() to hook_CreateFile2().\n");
        return false;
    }
    printf("Plugin Manager: Enabled 1 hook.\n");
    return true;
}
