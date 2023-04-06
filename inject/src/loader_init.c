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
#include "plugins.h"

static const char plugin_manager_msg[] = "[\033[32mPlugin Manager\033[0m]";

void loader_init() {
    console_setup(32000);
    SetConsoleTitle("Phantom Dust Plugin Console");
    printf("%s: Created console.\n", plugin_manager_msg);
    printf("%s: Initializing hooks...\n", plugin_manager_msg);
    hooks_setup_lock_files();
    vfs_setup();

    printf("%s: Finished startup. Welcome to Phantom Dust Plugin Manager.\n\n", plugin_manager_msg);

    inject_plugins();
    hooks_unlock_filesystem();

    const uintptr_t esper_base = (uintptr_t) GetModuleHandleA("PDUWP.exe");
    uint32_t* version_number = (uint32_t*)(uintptr_t)(esper_base + 0x4C5250);

	while (true) {
        // The version number is also reset like this whenever the game tries to check it.
        if (*version_number <= 140) {
            *version_number = 0;
        }
		Sleep(1);
	}
}
