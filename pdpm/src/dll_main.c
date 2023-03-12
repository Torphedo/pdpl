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
#include "path.h"
#include "hooks.h"
#include "filesystem.h"
#include "injection.h"

static const char plugin_manager_msg[] = "[\033[32mPlugin Manager\033[0m]";

void __stdcall injected(HMODULE dll_handle) {
    console_setup(32000, CONSOLE_CREATE);
    SetConsoleTitle("Phantom Dust Plugin Console");
    printf("%s: Created console.\n", plugin_manager_msg);
    printf("%s: Initializing hooks...\n", plugin_manager_msg);
    hooks_setup_lock_files();
    vfs_setup();

    inject_plugins();

    printf("%s: Finished startup. Welcome to Phantom Dust Plugin Manager.\n\n", plugin_manager_msg);

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
