// Reduce the size of Windows.h to improve compile time
#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB
#include <windows.h>
#include <tlhelp32.h>

#include "self_inject.h"

typedef struct BASE_RELOCATION_ENTRY {
    USHORT Offset : 12;
    USHORT Type : 4;
}BASE_RELOCATION_ENTRY;

uint32_t get_pid_by_name(const char* ProcessName) {
    PROCESSENTRY32 pt = {
            .dwSize = sizeof(PROCESSENTRY32)
    };
    HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32First(hsnap, &pt)) { // must call this first
        while (Process32Next(hsnap, &pt))
        {
            if (!lstrcmpi(pt.szExeFile, ProcessName)) {
                CloseHandle(hsnap);
                return pt.th32ProcessID;
            }
        }
    }
    CloseHandle(hsnap); // close handle on failure
    return 0;
}

bool self_inject(uint32_t process_id, LPTHREAD_START_ROUTINE entry_point, void* parameter) {
    // Open the target process - this is the process we will be injecting this PE into
    HANDLE target_process = OpenProcess(PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION, FALSE, process_id);
	if (target_process == INVALID_HANDLE_VALUE) {
	  return false;
	}

    // Get current image's base address
    uint8_t* image_base = GetModuleHandle(NULL);
    IMAGE_DOS_HEADER* dos_header = image_base;
    IMAGE_NT_HEADERS* nt_header = (image_base + dos_header->e_lfanew);

	// Allocate buffers for the PE data in the local and remote processes
    uint8_t* local_image = malloc(nt_header->OptionalHeader.SizeOfImage);
    uint8_t* target_image = VirtualAllocEx(target_process, NULL, nt_header->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	// Early exit if we fail an allocation. Free does nothing if passed NULL.
	if (local_image == NULL || target_image == NULL) {
	  free(local_image);
	  free(target_image);
	  CloseHandle(target_process);
	  return false;
	}

	// Copy PE data into our local buffer
    memcpy(local_image, image_base, nt_header->OptionalHeader.SizeOfImage);

    // Calculate delta between the base addresses of our two image addresses.
    uintptr_t delta_image_base = (uintptr_t)target_image - (uintptr_t)image_base;

    // Relocate local_image, to ensure that it will have correct addresses once it's in the target process
    IMAGE_BASE_RELOCATION* relocation_table = (IMAGE_BASE_RELOCATION*)(local_image + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

    while (relocation_table->SizeOfBlock > 0) {
	  	// BASE_RELOCATION_ENTRY is 2 bytes, so the size (minus header) over 2 is our entry count
		// (not using sizeof(BASE_RELOCATION_ENTRY) because some compilers might add padding or something on bitfields)
        uint32_t relocation_entries_count = (relocation_table->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
		// This VirtualAddress member tells us the offset of the relocation table from the image base.
        BASE_RELOCATION_ENTRY* relative_addresses = (BASE_RELOCATION_ENTRY*)((uintptr_t)relocation_table->VirtualAddress + local_image);

        for (uint32_t i = 0; i < relocation_entries_count; i++) {
            if (relative_addresses[i].Offset) {
			  	// Get the address of the pointer we need to relocate, then add our image base delta.
                uintptr_t* patched_address = (uintptr_t*)(relative_addresses + relative_addresses[i].Offset);
                *patched_address += delta_image_base;
            }
        }
		// Go to the next relocation table and repeat. There is one table per 4KiB page.
        relocation_table = (IMAGE_BASE_RELOCATION*)((uintptr_t)relocation_table + relocation_table->SizeOfBlock);
    }

    // Write the relocated PE into the target process
    WriteProcessMemory(target_process, target_image, local_image, nt_header->OptionalHeader.SizeOfImage, NULL);
    free(local_image);

    // Start the injected PE inside the target process
    CreateRemoteThread(target_process, NULL, 0, (LPTHREAD_START_ROUTINE)((uint8_t*)entry_point + delta_image_base), parameter, 0, NULL);

    return true;
}
