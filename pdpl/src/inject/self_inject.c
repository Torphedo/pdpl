#include <Windows.h>

#include <tlhelp32.h>

#include "self_inject.h"
#include "../loader_init.h"

/*
 * get_pid_by_name(): Taken from Skill Editor (Torphedo)
 * https://github.com/Torphedo/SkillEditor/blob/main/src/memory-editing.cpp#L30-L45 (private repo)
*/
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

DWORD InjectionEntryPoint()
{
    CHAR moduleName[128] = "";
    GetModuleFileNameA(NULL, moduleName, sizeof(moduleName));
    MessageBoxA(NULL, moduleName, "Obligatory PE Injection", 0);
    return 0;
}

typedef struct BASE_RELOCATION_ENTRY {
    USHORT Offset : 12;
    USHORT Type : 4;
} BASE_RELOCATION_ENTRY, * PBASE_RELOCATION_ENTRY;

bool self_inject(HANDLE target) {

    // Get current image's base address
    PVOID image_base = GetModuleHandle(NULL);
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)image_base;
    PIMAGE_NT_HEADERS nt_header = (PIMAGE_NT_HEADERS)((DWORD_PTR)image_base + dos_header->e_lfanew);

    // Allocate a new memory block and copy the current PE image to this new memory block
    PVOID local_image = VirtualAlloc(NULL, nt_header->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_READWRITE);
    memcpy(local_image, image_base, nt_header->OptionalHeader.SizeOfImage);

    // Open the target process - this is process we will be injecting this PE into

    // Allocate a new memory block in the target process. This is where we will be injecting this PE
    PVOID target_image = VirtualAllocEx(target, NULL, nt_header->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Calculate delta between addresses of where the image will be located in the target process and where it's located currently
    DWORD_PTR delta_image_base = (DWORD_PTR)target_image - (DWORD_PTR)image_base;

    // Relocate localImage, to ensure that it will have correct addresses once its in the target process
    PIMAGE_BASE_RELOCATION relocation_table = (PIMAGE_BASE_RELOCATION)((DWORD_PTR)local_image + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    DWORD relocation_entries_count = 0;
    PDWORD_PTR patched_address;
    PBASE_RELOCATION_ENTRY relocation_rva = NULL;

    while (relocation_table->SizeOfBlock > 0) {
        relocation_entries_count = (relocation_table->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
        relocation_rva = (PBASE_RELOCATION_ENTRY)(relocation_table + 1);

        for (short i = 0; i < relocation_entries_count; i++) {
            if (relocation_rva[i].Offset) {
                patched_address = (PDWORD_PTR)((DWORD_PTR)local_image + relocation_table->VirtualAddress + relocation_rva[i].Offset);
                *patched_address += delta_image_base;
            }
        }
        relocation_table = (PIMAGE_BASE_RELOCATION)((DWORD_PTR)relocation_table + relocation_table->SizeOfBlock);
    }

    // Write the relocated localImage into the target process
    WriteProcessMemory(target, target_image, local_image, nt_header->OptionalHeader.SizeOfImage, NULL);

    // Start the injected PE inside the target process
    CreateRemoteThread(target, NULL, 0, (LPTHREAD_START_ROUTINE)((DWORD_PTR)loader_init + delta_image_base), NULL, 0, NULL);

    return true;
}
