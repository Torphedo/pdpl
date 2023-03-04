#include <stdio.h>
#include <stdlib.h>

// Reduce the size of Windows.h to improve compile time
#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB
#include <Windows.h>

#include "injection.h"

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printf("Usage:\n\n\tinject [dll file]\n\nExample:\n\n\tinject pdpm.dll\n");
        system("pause");
        return EXIT_FAILURE;
    }

    // Turn any relative paths received from the user into full paths
    char full_path[MAX_PATH] = { 0 };
    GetFullPathNameA(argv[1], MAX_PATH, full_path, NULL);

    // Kill Phantom Dust if it's already running
    uint32_t process_id = get_pid_by_name("PDUWP.exe");
    if (process_id != 0) {
        // Get a handle with permission to terminate the game
        void* process = OpenProcess(PROCESS_TERMINATE, false, process_id);
        if (process == NULL) {
            printf("main(): Unable to open process ID %d for termination.\n", process_id);
            return false;
        }
        // Terminate Phantom Dust with an exit code of 0
        TerminateProcess(process, 0);
    }

    // Run Phantom Dust
    system("explorer shell:AppsFolder\\Microsoft.MSEsper_8wekyb3d8bbwe!App");
    while (get_pid_by_name("PDUWP.exe") == 0) {
        Sleep(1);
    }
    process_id = get_pid_by_name("PDUWP.exe");

	if(!dll_inject_remote(process_id, full_path, MAX_PATH + 1)) {
		printf("Injecting mods into Phantom Dust... Failed.\n");
		system("pause");
		return EXIT_FAILURE;
	}
	printf("Injecting mods into Phantom Dust... Success!\n");

	return EXIT_SUCCESS;
}
