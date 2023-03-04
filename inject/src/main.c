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
        printf("Usage:\n\n\tinject [dll file] [executable target]\n\nExample:\n\n\tinject pdpm.dll PDUWP.exe\n");
        system("pause");
        return EXIT_FAILURE;
    }

    // Turn any relative paths received from the user into full paths
    char full_path[MAX_PATH] = { 0 };
    GetFullPathNameA(argv[1], MAX_PATH, full_path, NULL);

	printf("Injecting mods into Phantom Dust...\n");
	if(!dll_inject_remote(get_pid_by_name(argv[2]), full_path, MAX_PATH + 1))
	{
		printf("Failed.\n");
		system("pause");
		return EXIT_FAILURE;
	}
	printf("Success!\n");

	return EXIT_SUCCESS;
}
