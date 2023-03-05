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
        printf("Failed to add %s to the virtual filesystem. (%s)\n", app_path, PHYSFS_getLastError());
    }
    else {
        printf("Added directory to virtual filesystem at /Assets/Data/: %s\n", app_path);
    }

    setup_hooks();

    // Inject plugin DLL here.

    printf("Plugin Manager: Initialized. Welcome to Phantom Dust Plugin Manager.\n");

	while (true) {
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
