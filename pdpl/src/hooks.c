#include <stdint.h>

#include <MinHook.h>

#include "hooks.h"

// This function gets run whenever you start trying to search for a multiplayer lobby.
typedef void (*CHECK_VERSION)(void);
CHECK_VERSION addr_check_version = NULL;
CHECK_VERSION original_check_version = NULL;

// This function appears to broadcast the version number (and maybe other info) whenever you create or change
// your lobby's settings. (needs confirmation / testing)
typedef void (*CHECK_VERSION_CREATE)(void*, int);
CHECK_VERSION_CREATE addr_check_version_create = NULL;
CHECK_VERSION_CREATE original_check_version_create = NULL;

// The base address of "PDUWP.exe" in memory, stored here so that we don't need to get it again every time
static uintptr_t pduwp = 0;

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

bool hook_create_anti_cheat() {
    MH_Initialize();

    pduwp = (uintptr_t) GetModuleHandleA("PDUWP.exe");
    addr_check_version = (void*)(uintptr_t)(pduwp + 0x1826B0);
    MH_CreateHook(addr_check_version, &hook_check_version, (void**)&original_check_version);
    MH_EnableHook(addr_check_version);

    addr_check_version_create = (void*)(uintptr_t)(pduwp + 0x182B60);
    MH_CreateHook(addr_check_version_create, &hook_check_version_create, (void**)&original_check_version_create);
    MH_EnableHook(addr_check_version_create);

    return true;
}
