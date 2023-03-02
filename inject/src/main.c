#include <stdio.h>
#include <stdint.h>

#include <windows.h>

#include "injection.h"

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printf("Usage:\n\n\tinject [dll file] [executable target]\n\nExample:\n\n\tinject payload.dll PDUWP.exe\n");
        return EXIT_FAILURE;
    }

    char full_path[MAX_PATH] = { 0 };
    GetFullPathNameA(argv[1], MAX_PATH, full_path, NULL);

    // Enable VT100
	DWORD ConsoleMode;
	GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleMode);
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
