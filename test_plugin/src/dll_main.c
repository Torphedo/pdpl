#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB

#include <Windows.h>

void __stdcall plugin_main(HMODULE dll_handle) {
    printf("Test plugin: injection complete.\n");

	const uint8_t* client = (uint8_t*)GetModuleHandle("PDUWP.exe");
	const uint8_t* gsdata = (client + 0x4C5240);

	while (!GetAsyncKeyState(VK_END)) {
		Sleep(1);
        *(uint32_t*)&gsdata[0x10] = 140;
	}

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
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) plugin_main, dll_handle, 0, NULL);
	}
	return TRUE;
}
