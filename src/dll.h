#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef BOOL (WINAPI *dll_entry)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

bool call_dll_main_remote(uint32_t pid, dll_entry dll_main, void* base_addr);

/// Extremely simple DLL injector using CreateRemoteThread() to call
/// LoadLibraryA([dll_path]) in the remote process.
/// \param pid ID of the target process
/// \param dll_path Path of the DLL to load
/// \return
bool remote_load_library(uint32_t pid, const char* dll_path);

/// Extremely simple DLL injector using CreateRemoteThread() to call
/// LoadLibraryA([dll_path]) in the remote process.
/// \param h A process handle with at least PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION
/// \param dll_path Path of the DLL to load
bool remote_load_library_existing(HANDLE h, const char* dll_path);

/// More advanced DLL injector that manually handles relocations and imports.
/// Does not require a file on disk and will not add the DLL to the module list.
/// \param pid ID of the target process</param>
/// \param dll_data Buffer with the contents of a DLL file</param>
bool dll_inject_memory(uint32_t pid, uint8_t* dll_data);

/// Convenience wrapper around dll_inject_memory() which passes it the contents
/// of a file as a buffer.
/// \param pid ID of the target process</param>
/// \param path Path of the file to read into memory and inject</param>
bool dll_inject_memory_file(uint32_t pid, const char* path);

