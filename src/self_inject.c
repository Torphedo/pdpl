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
} BASE_RELOCATION_ENTRY, * PBASE_RELOCATION_ENTRY;

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
    // Open the target process - this is process we will be injecting this PE into
    HANDLE targetProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION, FALSE, process_id);

    // Get current image's base address
    void* imageBase = GetModuleHandle(NULL);
    IMAGE_DOS_HEADER* dosHeader = imageBase;
    IMAGE_NT_HEADERS* ntHeader = (imageBase + dosHeader->e_lfanew);

    // Allocate a new memory block and copy the current PE image to this new memory block
    void* localImage = VirtualAlloc(NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_READWRITE);
    memcpy(localImage, imageBase, ntHeader->OptionalHeader.SizeOfImage);

    // Allocate a new memory block in the target process. This is where we will be injecting this PE
    void* targetImage = VirtualAllocEx(targetProcess, NULL, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Calculate delta between addresses of where the image will be located in the target process and where it's located currently
    uintptr_t deltaImageBase = (uintptr_t)targetImage - (uintptr_t)imageBase;

    // Relocate localImage, to ensure that it will have correct addresses once it's in the target process
    IMAGE_BASE_RELOCATION* relocationTable = (IMAGE_BASE_RELOCATION*)(localImage + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    DWORD relocationEntriesCount = 0;

    while (relocationTable->SizeOfBlock > 0)
    {
        relocationEntriesCount = (relocationTable->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);
        BASE_RELOCATION_ENTRY* relocationRVA = (BASE_RELOCATION_ENTRY*)(relocationTable + 1);

        for (short i = 0; i < relocationEntriesCount; i++)
        {
            if (relocationRVA[i].Offset)
            {
                uintptr_t* patchedAddress = (uintptr_t*)(localImage + relocationTable->VirtualAddress + relocationRVA[i].Offset);
                *patchedAddress += deltaImageBase;
            }
        }
        relocationTable = (IMAGE_BASE_RELOCATION*)((uintptr_t)relocationTable + relocationTable->SizeOfBlock);
    }

    // Write the relocated localImage into the target process
    WriteProcessMemory(targetProcess, targetImage, localImage, ntHeader->OptionalHeader.SizeOfImage, NULL);

    // Start the injected PE inside the target process
    CreateRemoteThread(targetProcess, NULL, 0, (LPTHREAD_START_ROUTINE)((DWORD_PTR)entry_point + deltaImageBase), parameter, 0, NULL);

    return true;
}
