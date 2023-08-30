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
    PROCESSENTRY32 entry = {
            .dwSize = sizeof(PROCESSENTRY32)
    };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32First(snapshot, &entry)) { // must call this first
        while (Process32Next(snapshot, &entry))
        {
            if (!lstrcmpi(entry.szExeFile, ProcessName)) {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        }
    }
    CloseHandle(snapshot); // close handle on failure
    return 0;
}

bool self_inject(uint32_t process_id, LPTHREAD_START_ROUTINE entry_point, void* parameter) {
    // Open the target process - this is the process we will be injecting this PE into
    HANDLE target_process = OpenProcess(PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION, FALSE, process_id);
	if (target_process == INVALID_HANDLE_VALUE) {
	  return false;
	}

    // Get current image's base address
    uint8_t* image_base = (uint8_t*) GetModuleHandle(NULL);
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*) image_base;
    IMAGE_NT_HEADERS* nt_header = (IMAGE_NT_HEADERS*) (image_base + dos_header->e_lfanew);

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
    uintptr_t reloc_bias = (uintptr_t)target_image - (uintptr_t)image_base;

    // Relocate local_image, to ensure that it will have correct addresses once it's in the target process
    IMAGE_BASE_RELOCATION* reloc_table = (IMAGE_BASE_RELOCATION*)(local_image + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

    while (reloc_table->SizeOfBlock > 0) {
	  	// BASE_RELOCATION_ENTRY is 2 bytes, so the size (minus header) over 2 is our entry count
		// (not using sizeof(BASE_RELOCATION_ENTRY) because some compilers might add padding or something on bitfields)
        uint32_t entries_count = (reloc_table->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
		// This VirtualAddress member tells us the offset of the relocation table from the image base.
        BASE_RELOCATION_ENTRY* relative_addresses = (BASE_RELOCATION_ENTRY*)((uintptr_t)reloc_table->VirtualAddress + local_image);

        for (uint32_t i = 0; i < entries_count; i++) {
            if (relative_addresses[i].Offset) {
			  	// Get the address of the pointer we need to relocate, then add our image base delta.
                uintptr_t* patched_address = (uintptr_t*)(relative_addresses + relative_addresses[i].Offset);
                *patched_address += reloc_bias;
            }
        }
		// Go to the next relocation table and repeat. There is one table per 4KiB page.
        reloc_table = (IMAGE_BASE_RELOCATION*)((uintptr_t)reloc_table + reloc_table->SizeOfBlock);
    }

    // Write the relocated PE into the target process
    WriteProcessMemory(target_process, target_image, local_image, nt_header->OptionalHeader.SizeOfImage, NULL);
    free(local_image);

    // Start the injected PE inside the target process
    CreateRemoteThread(target_process, NULL, 0, (LPTHREAD_START_ROUTINE)((uint8_t*)entry_point + reloc_bias), parameter, 0, NULL);
	CloseHandle(target_process);

    return true;
}
