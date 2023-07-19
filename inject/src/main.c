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
#include <shlobj.h> // SHGetFolderPathA()
#include <sys/stat.h>
#include <direct.h> // _mkdir()

#include "remote_injection.h"

bool file_exists(const char* path) {
    struct stat buffer = {0};
    return (stat(path, &buffer) == EXIT_SUCCESS);
}

int main(int argc, char** argv) {
    static int result = EXIT_SUCCESS;

    // Copy pd_loader_core.dll to a place where it can be read by the game, if it doesn't exist there.
    static char mods_path[MAX_PATH] = {0};
    SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA, NULL, 0, mods_path);
    strcat(mods_path, "\\Packages\\Microsoft.MSEsper_8wekyb3d8bbwe\\RoamingState\\mods");
    _mkdir(mods_path);
    strcat(mods_path, "\\pd_loader_core.dll");
    if (!file_exists(mods_path)) {
        if (!file_exists("pd_loader_core.dll")) {
            printf("Couldn't find pd_loader_core.dll. Please place this file in the mods folder or next to pdpl.exe.\n");
            system("pause");
        }
        else {
            printf("Copying pd_loader_core.dll into the mods folder for first-time setup.\n");
            CopyFile("pd_loader_core.dll", mods_path, FALSE);
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

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, process_id);

    printf("Injecting mods into Phantom Dust...");
    if(process == INVALID_HANDLE_VALUE) {
        printf("Failed (INVALID_HANDLE_VALUE).\n");
        result = EXIT_FAILURE;
    }
    else {
        if(!ManualMapDll(process, true, true, true, true)) {
            printf("Failed (Couldn't inject the bootstrap program).\n");
            result = EXIT_FAILURE;
        }
        else {
            printf("Success!\n");
            result = EXIT_SUCCESS;
        }
        CloseHandle(process);
    }

    if (result != EXIT_SUCCESS) {
        system("pause");
    }
	return result;
}
