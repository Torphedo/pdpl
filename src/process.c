#include <stdint.h>
#include <stdbool.h>

#include <windows.h>
#include <psapi.h>

#include <common/path.h>
#include <common/logging.h>
#include "process.h"

bool enable_debug_privilege(bool bEnable) {
	HANDLE hToken = NULL;
	LUID luid;
	
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		return FALSE;
	}
	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
		return FALSE;
	}

	TOKEN_PRIVILEGES tokenPriv = {
		.PrivilegeCount = 1,
		.Privileges[0] = {
			.Luid = luid,
			.Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0
		}
	};
	if (!AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
		return FALSE;
	}
	return TRUE;
}

void* get_remote_module(const char* module_name, uint32_t pid) {
	HANDLE target = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (target == INVALID_HANDLE_VALUE) {
		LOG_MSG(error, "Failed to open process %d to get module list\n", pid);
		return NULL;
	}

	// Find out how much space we actually need. Hate this API.
	HMODULE dummy = 0;
	DWORD modules_size = 0;
	EnumProcessModules(target, &dummy, sizeof(HMODULE), &modules_size);

	HMODULE* modules = calloc(1, modules_size);
	if (modules == NULL) {
		CloseHandle(target);
		return NULL;
	}
	EnumProcessModules(target, modules, modules_size, &modules_size);

	char module_path[MAX_PATH] = {0};
	HMODULE output = 0;
	uint64_t module_count = (modules_size / sizeof(HMODULE)) + 1;
	for (uint32_t i = 0; i < module_count; i++) {
		GetModuleFileNameEx(target, modules[i], module_path, MAX_PATH);

		char module_filename[MAX_PATH] = {0};
		path_get_filename(module_path, module_filename);

		// strnicmp() is case-insensitive
		if (strnicmp(module_filename, module_name, MAX_PATH) == 0) {
			output = modules[i];
			break;
		}

		// Wipe the string so when we overwrite it again the null terminator
		// will be correct
		memset(module_path, 0x00, MAX_PATH);
	}

	free(modules);
	CloseHandle(target);

	return output;
}

void* get_remote_proc_addr(const char* module_name, const char* proc_name, uint32_t pid) {
	void* local_module = GetModuleHandle(module_name);
	if (local_module == NULL) {
		LOG_MSG(error, "Module %s not found in local process.\n", module_name);
		return NULL;
	}
	void* local_proc = GetProcAddress((void*)local_module, proc_name);
	if (local_proc == NULL) {
		LOG_MSG(error, "Couldn't find %s::%s() locally\n", module_name, proc_name);
		return NULL;
	}
	uintptr_t remote_module = (uintptr_t)get_remote_module(module_name, pid);

	// Offset of function in DLL
	uintptr_t proc_offset = ((uintptr_t)local_proc - (uintptr_t)local_module);

	return (void*)((uintptr_t)remote_module + proc_offset);
}

