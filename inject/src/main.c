#include <stdio.h>
#include <stdint.h>
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
        printf("Usage:\n\n\tinject [dll file] [executable target]\n\nExample:\n\n\tinject pdpm.dll PDUWP.exe\n");
        return EXIT_FAILURE;
    }

    // Turn any relative paths received from the user into full paths
    char full_path[MAX_PATH] = { 0 };
    GetFullPathNameA(argv[1], MAX_PATH, full_path, NULL);

    // Enable VT100 (ANSI escape codes)
	uint32_t ConsoleMode = 0;
	GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), (LPDWORD) &ConsoleMode);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	printf("\033[93mInjecting into remote process: ");
	if(!dll_inject_remote(get_pid_by_name(argv[2]), full_path, MAX_PATH + 1))
	{
		printf("\033[91mFailed\033[0m\n");
		system("pause");
		return EXIT_FAILURE;
	}
	printf("\033[92mSuccess!\033[0m\n");

	return EXIT_SUCCESS;
}
