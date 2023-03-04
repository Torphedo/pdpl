#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB

#include <Windows.h>
#include <physfs.h>

#include "console.h"
#include "hooks.h"
#include "path.h"

void __stdcall injected(HMODULE dll_handle) {
    console_setup(32000, CONSOLE_CREATE);
    SetConsoleTitle("Phantom Dust Plugin Console");
    printf("Plugin Manager: Created console.\n");
    setup_hooks();

    // We pass NULL for the "argv[0]" parameter because we don't have argv
    PHYSFS_init(NULL);
    printf("Plugin Manager: Initialized virtual filesystem.\n");

    // Get DLL's path
    static char module_path[MAX_PATH] = { 0 };
    GetModuleFileNameA(dll_handle, module_path, MAX_PATH);
    path_truncate(module_path, MAX_PATH + 1);

    printf("Plugin Folder: %s\n", module_path);
    if (path_is_plugin_folder(module_path)) {
        printf("Plugin Folder: Valid\n");
    }
    else {
        printf("Plugin Folder: Invalid\n");
    }

    strcat(module_path, "test_plugin.dll");
    printf("Plugin Manager: Loading plugin %s\n", module_path);
    // Inject plugin DLL here.

    printf("Plugin Manager: Initialized. Welcome to Phantom Dust Plugin Manager.\n");

	const uint8_t* client = (uint8_t*)GetModuleHandle("PDUWP.exe");
	const uint8_t* gsdata = (client + 0x4C5240);

	while (!GetAsyncKeyState(VK_END)) {
		Sleep(1000);
	}

    PHYSFS_deinit();

	// Uninject
	FreeLibraryAndExitThread((HMODULE)(dll_handle), EXIT_SUCCESS);
}

int32_t __stdcall DllMain(HINSTANCE dll_handle, uint32_t reason, void* reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// Disable DLL notifications for new threads starting up, because we have no need to run special code here.
		DisableThreadLibraryCalls(dll_handle);

		// Start injected code
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) injected, dll_handle, 0, NULL);
	}
	return TRUE;
}
