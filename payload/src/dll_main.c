#include <stdint.h>
#include <stdlib.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#include <MinHook.h>

void __stdcall injected(void* dll_handle) {
	const uint8_t* client = (uint8_t*)GetModuleHandle("PDUWP.exe");

	const uint8_t* gsdata = (client + 0x4C5240);

	while (!GetAsyncKeyState(VK_END))
	{
		Sleep(500);
		if (gsdata[0x10] == 0)
		{
			*(uint32_t*)&gsdata[0x10] = 140;
		}
		else
		{
			*(uint32_t*)&gsdata[0x10] = 0;
		}
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
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) injected, dll_handle, 0, NULL);
	}
	return TRUE;
}
