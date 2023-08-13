#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Reduce the size of Windows.h to improve compile time
#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB
#include <Windows.h>
#include <shlobj.h>
#include <direct.h>

#include "self_inject.h"
#include "path.h"
#include "hooks.h"

static char core_path[MAX_PATH] = {0};

DWORD bootstrap(void* loader_path) {
    hook_create_anti_cheat();
    LoadLibrary(core_path);

    // Get version number offset.
    const uintptr_t esper_base = (uintptr_t) GetModuleHandleA("PDUWP.exe");
    uint32_t* version_number = (uint32_t*)(uintptr_t)(esper_base + 0x4C5250);

    while (true) {
        // The version number is also reset like this (by our anti-cheat hooks) whenever the game tries to check it.
        if (*version_number <= 140) {
            *version_number = 0;
        }
        Sleep(1);
    }
}

int main(int argc, char** argv) {
    static int result = EXIT_SUCCESS;

    // Copy pd_loader_core.dll to a place where it can be read by the game, if it doesn't exist there.
    SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, 0, core_path);
    strcat(core_path, "\\Packages\\Microsoft.MSEsper_8wekyb3d8bbwe\\RoamingState\\mods");
    _mkdir(core_path);
    strcat(core_path, "\\pd_loader_core.dll");
    if (!file_exists(core_path)) {
        if (!file_exists("pd_loader_core.dll")) {
            printf("Couldn't find pd_loader_core.dll. Please place this file in the mods folder or next to the program.\n");
            system("pause");
        }
        else {
            printf("Copying pd_loader_core.dll into the mods folder for first-time setup.\n");
            CopyFile("pd_loader_core.dll", core_path, FALSE);
        }
    }

    // Kill Phantom Dust if it's already running
    uint32_t process_id = get_pid_by_name("PDUWP.exe");
    if (process_id != 0) {
        // Get a handle with permission to terminate the game
        void* process = OpenProcess(PROCESS_TERMINATE, false, process_id);
        if (process == NULL) {
            printf("main(): Unable to open process ID %d for termination.\n", process_id);
            system("pause");
            return EXIT_FAILURE;
        }
        // Terminate Phantom Dust with a success error code
        TerminateProcess(process, EXIT_SUCCESS);
        CloseHandle(process);
    }

    // Run Phantom Dust
    system("explorer shell:AppsFolder\\Microsoft.MSEsper_8wekyb3d8bbwe!App");

    // Wait for the game to start, so we can get a handle to it
    while (get_pid_by_name("PDUWP.exe") == 0) {
        Sleep(1);
    }

    process_id = get_pid_by_name("PDUWP.exe");

    printf("Injecting mods into Phantom Dust...");
    self_inject(process_id, bootstrap, NULL);

    if (result != EXIT_SUCCESS) {
        system("pause");
    }
	return result;
}
