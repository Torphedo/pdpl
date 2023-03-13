#include <stdio.h>

// Allow use of LPCREATEFILE2_EXTENDED_PARAMETERS on MinGW
#define _WIN32_WINNT 0x0603

#include <fileapi.h>
#include <MinHook.h>
#include <physfs.h>

#include "hooks.h"
#include "path.h"
#include "atomic_log.h"

// These are both void*
typedef struct {
    PHYSFS_File* physfs_handle;
    HANDLE win_handle;
}file_handle;

// Stores pairs of PhysicsFS and Windows file handles.
// Enough for the game to open 32,768 files at once. There are roughly 3,800 files in the game.
static file_handle handle_list[0x8000] = {0};
static uint16_t handle_list_pos = 0;
// Stores which handle pairs are free to be overwritten (by index). When a handle is closed, it's added to this list and the handle pair is set to NULL.
// When a free handle pair is overwritten, its entry in this list is set to 0xFFFF. The list should be initialized to 0xFFFF.
static uint16_t* free_list = NULL;
static uint16_t free_count = 0;

typedef HANDLE (*CREATE_FILE_2)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
CREATE_FILE_2 original_CreateFile2 = NULL;
CREATE_FILE_2 addr_CreateFile2 = NULL;

typedef int (*READ_FILE)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
READ_FILE addr_ReadFile = NULL;
READ_FILE original_ReadFile = NULL;

// This function gets run whenever you start trying to search for a multiplayer lobby.
typedef void (*CHECK_VERSION)(void);
CHECK_VERSION addr_check_version = NULL;
CHECK_VERSION original_check_version = NULL;

// This function appears to broadcast the version number (and maybe other info) whenever you create or change
// your lobby's settings. (needs confirmation / testing)
typedef void (*CHECK_VERSION_CREATE)(void*, int);
CHECK_VERSION_CREATE addr_check_version_create = NULL;
CHECK_VERSION_CREATE original_check_version_create = NULL;

bool lock_filesystem = true;

// The base address of "PDUWP.exe" in memory, stored here so that we don't need to get it again every time
static uintptr_t pduwp = 0;

HANDLE hook_CreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams) {
    // During startup, it seems that this function is sometimes called asynchronously. This value is shared across
    // each of these running instances, so while the function is running it will force other instances of the call to wait.
    static bool lock_async_calls = false;
    while (lock_async_calls) {
        Sleep(1);
    }
    lock_async_calls = true;

    char path[MAX_PATH] = {0};
    wchar_t wide_path[MAX_PATH] = {0};
    PHYSFS_utf8FromUtf16(lpFileName, path, MAX_PATH);
    path_make_physfs_friendly(path);

    static file_handle handles = {0};

    if (PHYSFS_exists(path)) {
        // Get the real path of the file and place it in a wide string.
        swprintf(wide_path, MAX_PATH, L"%hs/%hs", PHYSFS_getRealDir(path), path);
        handles.win_handle = original_CreateFile2(wide_path, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
    }
    else {
        handles.win_handle = original_CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
    }

    MH_DisableHook(addr_CreateFile2);
    printf("CreateFile2() ");
    switch(dwDesiredAccess) {
        case GENERIC_READ:
            printf("[\033[32mGENERIC_READ\033[0m]:");
            handles.physfs_handle = PHYSFS_openRead(path);
            break;
        case GENERIC_WRITE:
            printf("[\033[31mGENERIC_WRITE\033[0m]:");
            handles.physfs_handle = PHYSFS_openWrite(path);
            break;
        case (GENERIC_READ | GENERIC_WRITE):
            printf("[\033[33mGENERIC_READ | GENERIC_WRITE\033[0m]:");
            handles.physfs_handle = PHYSFS_openWrite(path);
            break;
        default:
            printf("[UNKNOWN]:");
    }
    printf(" Opened %s\n", path);

    if (PHYSFS_exists(path) && handles.win_handle == INVALID_HANDLE_VALUE) {
        printf("Creating a fake file for Windows handle.\n");
        system("pause");
        static char fake_path[MAX_PATH] = {0};
        get_ms_esper_path(fake_path);
        {
            static char filename[MAX_PATH] = {0}; // Temporary buffer to store filename
            path_get_filename(path, filename);
            sprintf(fake_path, "%sfake\\%s", fake_path, filename);
        }

        // Create a new file with the appropriate size in the /fake/ folder.
        FILE* fake_file = fopen(fake_path, "wb");
        fseek(fake_file, PHYSFS_fileLength(handles.physfs_handle), SEEK_SET);
        fwrite("\0", sizeof("\0"), 1, fake_file);
        fclose(fake_file);

        PHYSFS_utf8ToUtf16(fake_path, wide_path, MAX_PATH);
        handles.win_handle = original_CreateFile2(wide_path, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
        printf("PhysicsFS handle was %p.\n", handles.physfs_handle);
    }

    MH_EnableHook(addr_CreateFile2);

    if (free_count == 0) {
        // Insert new entry in handle list and increment position
        handle_list[handle_list_pos++] = handles;
    }
    else {
        uint16_t index = free_list[--free_count];
        free_list[free_count] = 0xFFFF;
        handle_list[index] = handles;
    }
    lock_async_calls = false; // Allow waiting call to run.
    return handles.win_handle;
}

typedef FILE* (*FOPEN)(const char* path, const char* mode);
FOPEN addr_fopen = NULL;
FOPEN original_fopen = NULL;
FILE* hook_fopen(const char* path, const char* mode) {
    char virtual_path[MAX_PATH] = {0};
    if (PHYSFS_exists(path)) {
        // Get the real path of the file and place it in a wide string.
        sprintf(virtual_path,  "%s\\%s", PHYSFS_getRealDir(path), path);
        return original_fopen(virtual_path, mode);
    }
    else {
        return original_fopen(path, mode);
    }
}

// Stalls until filesystem access is unlocked, then tries to call the original function (which now redirects to hook_CreateFile2()).
HANDLE hook_CreateFile2_stall(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams) {
    while(lock_filesystem) {
        printf("CreateFile2(): Waiting for virtual filesystem to start...\n");
        Sleep(50);
    }
    return addr_CreateFile2(lpFileName, dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
}

int hook_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    static bool lock_async_calls = false;
    while (lock_async_calls) {
        Sleep(1);
    }
    lock_async_calls = true;

    file_handle handles = {0};
    for (uint32_t i = 0; i < handle_list_pos; i++) {
        if (hFile == handle_list[i].win_handle) {
           handles = handle_list[i];
        }
    }
    int result = original_ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

    if (handles.physfs_handle != NULL) {
        if (lpOverlapped != NULL) {
            PHYSFS_seek(handles.physfs_handle, (uint64_t) lpOverlapped->Offset | (uint64_t) lpOverlapped->OffsetHigh);
        }
        // PHYSFS_readBytes(handles.physfs_handle, lpBuffer, nNumberOfBytesToRead);
    }

    lock_async_calls = false; // Allow waiting call to run.
    return result;
}

void hooks_unlock_filesystem() {
    // atomic_log("\n\n=== Phantom Dust Plugin Manager ===\n\nStarted virtual filesystem.\n", "plugin_manager.log");
    if (MH_RemoveHook(addr_CreateFile2) != MH_OK) {
        printf("Failed to remove stall hook.\n");
    }
    if (MH_CreateHook(addr_CreateFile2, &hook_CreateFile2, (void**)&original_CreateFile2) != MH_OK) {
        printf("Failed to create real hook.\n");
    }

    MH_EnableHook(MH_ALL_HOOKS);
    lock_filesystem = false;
}

void* get_procedure_address(const wchar_t* module_name, const char* proc_name) {
    HMODULE hModule = GetModuleHandleW(module_name);
    if (hModule == NULL) {
        return NULL;
    }
    return GetProcAddress(hModule, proc_name);
}

void hook_check_version(void) {
    uint32_t* version_number = (uint32_t*)(uintptr_t)(pduwp + 0x4C5250);
    if (*version_number <= 140) {
        *version_number = 0;
    }
    return original_check_version();
}

void hook_check_version_create(void* unknown_ptr, int unknown_int) {
    uint32_t* version_number = (uint32_t*)(uintptr_t)(pduwp + 0x4C5250);
    if (*version_number <= 140) {
        *version_number = 0;
    }
    return original_check_version_create(unknown_ptr, unknown_int);
}

bool hooks_setup_lock_files() {
    MH_Initialize();

    addr_CreateFile2 = get_procedure_address(L"KERNELBASE.dll", "CreateFile2");
    addr_ReadFile = get_procedure_address(L"KERNELBASE.dll", "ReadFile");
    addr_fopen = get_procedure_address(L"msvcrt.dll", "fopen");

    if (MH_CreateHook(addr_CreateFile2, &hook_CreateFile2_stall, (void**)&original_CreateFile2) != MH_OK) {
        printf("Failed to create hook.\n");
        return false;
    }

    if (MH_CreateHook(addr_ReadFile, &hook_ReadFile, (void**)&original_ReadFile) != MH_OK) {
        printf("Failed to create hook.\n");
        return false;
    }

    if (MH_EnableHook(addr_CreateFile2) != MH_OK) {
        printf("Failed to enable hook.\n");
        return false;
    }

    if (MH_CreateHook(addr_fopen, &hook_fopen, (void**)&original_fopen) != MH_OK) {
        printf("Failed to create hook for fopen().\n");
    }
    if(MH_EnableHook(addr_fopen) != MH_OK) {
        printf("Failed to enable hook for fopen().\n");
    }

    pduwp = (uintptr_t) GetModuleHandleA("PDUWP.exe");
    addr_check_version = (void*)(uintptr_t)(pduwp + 0x1826B0);
    MH_CreateHook(addr_check_version, &hook_check_version, (void**)&original_check_version);
    MH_EnableHook(addr_check_version);

    addr_check_version_create = (void*)(uintptr_t)(pduwp + 0x182B60);
    MH_CreateHook(addr_check_version_create, &hook_check_version_create, (void**)&original_check_version_create);
    MH_EnableHook(addr_check_version_create);

    return true;
}
