#include <stdint.h>
#include <stdbool.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void* get_remote_proc_addr(const char* module_name, const char* proc_name, HANDLE h);

/// Requires a process handle with PROCESS_QUERY_INFORMATION & PROCESS_VM_READ
void* get_remote_module(const char* module_name, HANDLE h);

bool enable_debug_privilege(bool bEnable);

