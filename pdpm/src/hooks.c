#include <stdio.h>

// Allow use of LPCREATEFILE2_EXTENDED_PARAMETERS
#define _WIN32_WINNT 0x0603

#include <fileapi.h>
#include <MinHook.h>
#include <physfs.h>

#include "hooks.h"
#include "path.h"

typedef HANDLE (*CREATE_FILE_2)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
CREATE_FILE_2 original_CreateFile2 = NULL;
CREATE_FILE_2 addr_CreateFile2 = NULL;

bool lock_filesystem = true;

HANDLE hook_CreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams) {
    char filename[MAX_PATH] = {0};
    PHYSFS_utf8FromUtf16(lpFileName, filename, MAX_PATH);
    path_make_physfs_friendly(filename);

    printf("CreateFile2() ");

    if (dwDesiredAccess == GENERIC_READ) {
        printf("[\033[32mGENERIC_READ\033[0m]");
    }
    else if (dwDesiredAccess == GENERIC_WRITE) {
        printf("[\033[31mGENERIC_WRITE\033[0m]");
    }
    else if (dwDesiredAccess == (GENERIC_READ | GENERIC_WRITE)) {
        printf("[\033[33mGENERIC_READ | GENERIC_WRITE\033[0m]");
    }
    printf(": Opened %s\n", filename);

    if (PHYSFS_exists(filename)) {
        printf("[File found in modded filesystem, unable to load mod files in this build]\n");
    }
    return original_CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
}

// Stalls until filesystem access is unlocked, then tries to call the original function (which now redirects to hook_CreateFile2()).
HANDLE hook_CreateFile2_stall(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams) {
    while(lock_filesystem) {
        printf("CreateFile2(): Waiting for virtual filesystem to start...\n");
        Sleep(50);
    }
    return addr_CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
}

void hooks_unlock_filesystem() {
    if (MH_RemoveHook(addr_CreateFile2) != MH_OK) {
        printf("Failed to remove stall hook.\n");
    }
    if (MH_CreateHook(addr_CreateFile2, &hook_CreateFile2, (void**)&original_CreateFile2) != MH_OK) {
        printf("Failed to create real hook.\n");
    }
    MH_EnableHook(addr_CreateFile2);
    lock_filesystem = false;
}

void* get_procedure_address(const wchar_t* module_name, const char* proc_name) {
    HMODULE hModule = GetModuleHandleW(module_name);
    if (hModule == NULL) {
        return NULL;
    }
    return GetProcAddress(hModule, proc_name);
}

bool hooks_setup_lock_files() {
    MH_Initialize();

    addr_CreateFile2 = get_procedure_address(L"KERNELBASE.dll", "CreateFile2");

    if (MH_CreateHook(addr_CreateFile2, &hook_CreateFile2_stall, (void**)&original_CreateFile2) != MH_OK) {
        printf("Failed to create hook.\n");
        return false;
    }

    if (MH_EnableHook(addr_CreateFile2) != MH_OK) {
        printf("Failed to enable hook.\n");
        return false;
    }
    return true;
}
