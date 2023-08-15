#include <stdio.h>
#include <aclapi.h>
#include <sddl.h>

// Reduce the size of Windows.h to improve compile time
#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB
#include <windows.h>

#include <tlhelp32.h>
#include <incbin.h>

#include "remote_injection.h"

INCBIN(uint8_t, dll_file, PDPM_DLL);

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

bool ManualMapDll(HANDLE process, bool ClearHeader, bool ClearNonNeededSections, bool AdjustProtections, bool SEHExceptionSupport) {
    uint8_t* dll_data = (uint8_t*)&gdll_fileData;

    IMAGE_NT_HEADERS* old_nt_header = (IMAGE_NT_HEADERS*)(dll_data + ((IMAGE_DOS_HEADER*)(dll_data))->e_lfanew);
    IMAGE_OPTIONAL_HEADER* old_opt_header = &old_nt_header->OptionalHeader;
    IMAGE_FILE_HEADER* old_file_header = &old_nt_header->FileHeader;

    uint8_t* target_base = (uint8_t*)(VirtualAllocEx(process, NULL, old_opt_header->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!target_base) {
        printf("Target process memory allocation failed (ex) 0x%lx\n", GetLastError());
        return false;
    }

    unsigned long old = 0; // This isn't used but VirtualProtectEx requires a valid pointer
    VirtualProtectEx(process, target_base, old_opt_header->SizeOfImage, PAGE_EXECUTE_READWRITE, &old);

    MANUAL_MAPPING_DATA data = {
            .pLoadLibraryA = LoadLibraryA,
            .pGetProcAddress = GetProcAddress,
            .pRtlAddFunctionTable = (f_RtlAddFunctionTable) RtlAddFunctionTable,
            .pbase = target_base,
    };

    // File header
    if (!WriteProcessMemory(process, target_base, dll_data, 0x1000, NULL)) { //only first 0x1000 bytes for the header
        printf("Injector: failed to write header (error code 0x%lx)\n", GetLastError());
        VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
        return false;
    }

    IMAGE_SECTION_HEADER* section_header = IMAGE_FIRST_SECTION(old_nt_header);
    for (uint32_t i = 0; i != old_file_header->NumberOfSections; ++i, ++section_header) {
        if (section_header->SizeOfRawData) {
            if (!WriteProcessMemory(process, target_base + section_header->VirtualAddress, dll_data + section_header->PointerToRawData, section_header->SizeOfRawData, NULL)) {
                printf("Injector: failed to map image sections (error code 0x%lx)\n", GetLastError());
                VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
                return false;
            }
        }
    }

    // Mapping params
    uint8_t* mapping_data = (uint8_t*)(VirtualAllocEx(process, NULL, sizeof(MANUAL_MAPPING_DATA), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!mapping_data) {
        printf("Injector: mapping allocation failed (error code 0x%lx)\n", GetLastError());
        VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
        return false;
    }

    if (!WriteProcessMemory(process, mapping_data, &data, sizeof(MANUAL_MAPPING_DATA), NULL)) {
        printf("Injector: failed to write mapping (error code 0x%lx)\n", GetLastError());
        VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
        VirtualFreeEx(process, mapping_data, 0, MEM_RELEASE);
        return false;
    }

    // Shell code
    void* pShellcode = VirtualAllocEx(process, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!pShellcode) {
        printf("Injector: failed to allocate bootstrapper code (error code 0x%lx)\n", GetLastError());
        VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
        VirtualFreeEx(process, mapping_data, 0, MEM_RELEASE);
        return false;
    }

    if (!WriteProcessMemory(process, pShellcode, Shellcode, 0x1000, NULL)) {
        printf("Injector: failed to write bootstrapper code (error code 0x%lx)\n", GetLastError());
        VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
        VirtualFreeEx(process, mapping_data, 0, MEM_RELEASE);
        VirtualFreeEx(process, pShellcode, 0, MEM_RELEASE);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)(pShellcode), mapping_data, 0, NULL);
    if (!thread) {
        printf("Injector: thread creation failed (error code 0x%lx)\n", GetLastError());
        VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
        VirtualFreeEx(process, mapping_data, 0, MEM_RELEASE);
        VirtualFreeEx(process, pShellcode, 0, MEM_RELEASE);
        return false;
    }
    CloseHandle(thread);

    HINSTANCE hCheck = NULL;
    while (!hCheck) {
        unsigned long exitcode = 0;
        GetExitCodeProcess(process, &exitcode);
        if (exitcode != STILL_ACTIVE) {
            printf("Process crashed, exit code: %li\n", exitcode);
            return false;
        }

        MANUAL_MAPPING_DATA data_checked = { 0 };
        ReadProcessMemory(process, mapping_data, &data_checked, sizeof(data_checked), NULL);
        hCheck = data_checked.hMod;

        if (hCheck == (HINSTANCE)0x404040) {
            VirtualFreeEx(process, target_base, 0, MEM_RELEASE);
            VirtualFreeEx(process, mapping_data, 0, MEM_RELEASE);
            VirtualFreeEx(process, pShellcode, 0, MEM_RELEASE);
            return false;
        }
        else if (hCheck == (HINSTANCE)0x505050) {
            printf("Injector (warning): exception support failed!\n");
        }

        Sleep(10);
    }

    uint8_t* empty_buffer = malloc(1024 * 1024 * 20); // 20MiB
    if (empty_buffer == NULL) {
        printf("Unable to allocate memory\n");
        return false;
    }
    memset(empty_buffer, 0, 1024 * 1024 * 20);

    // Clear PE Header
    if (ClearHeader) {
        if (!WriteProcessMemory(process, target_base, empty_buffer, 0x1000, NULL)) {
            printf("Injector: failed to clear header\n");
        }
    }


    if (ClearNonNeededSections) {
        section_header = IMAGE_FIRST_SECTION(old_nt_header);
        for (uint32_t i = 0; i != old_file_header->NumberOfSections; ++i, ++section_header) {
            if (section_header->Misc.VirtualSize) {
                if ((SEHExceptionSupport ? 0 : strcmp((char*)section_header->Name, ".pdata") == 0) ||
                    strcmp((char*)section_header->Name, ".rsrc") == 0 ||
                    strcmp((char*)section_header->Name, ".reloc") == 0) {
                    if (!WriteProcessMemory(process, target_base + section_header->VirtualAddress, empty_buffer, section_header->Misc.VirtualSize, NULL)) {
                        printf("Injector: failed to clear section %s: 0x%lx\n", section_header->Name, GetLastError());
                    }
                }
            }
        }
    }

    if (AdjustProtections) {
        section_header = IMAGE_FIRST_SECTION(old_nt_header);
        for (uint32_t i = 0; i != old_file_header->NumberOfSections; ++i, ++section_header) {
            if (section_header->Misc.VirtualSize) {
                DWORD newP = PAGE_READONLY;

                if ((section_header->Characteristics & IMAGE_SCN_MEM_WRITE) > 0) {
                    newP = PAGE_READWRITE;
                }
                else if ((section_header->Characteristics & IMAGE_SCN_MEM_EXECUTE) > 0) {
                    newP = PAGE_EXECUTE_READ;
                }
                if (!VirtualProtectEx(process, target_base + section_header->VirtualAddress, section_header->Misc.VirtualSize, newP, &old)) {
                    printf("FAIL: section %s not set as %lx\n", (char*)section_header->Name, newP);
                }
            }
        }
        VirtualProtectEx(process, target_base, IMAGE_FIRST_SECTION(old_nt_header)->VirtualAddress, PAGE_READONLY, &old);
    }

    if (!WriteProcessMemory(process, pShellcode, empty_buffer, 0x1000, NULL)) {
        printf("Injector (warning): failed to clear bootstrapper code\n");
    }
    if (!VirtualFreeEx(process, pShellcode, 0, MEM_RELEASE)) {
        printf("Injector: failed to release bootstrapper code memory\n");
    }
    if (!VirtualFreeEx(process, mapping_data, 0, MEM_RELEASE)) {
        printf("Injector: failed to release mapping memory\n");
    }

    return true;
}

#define RELOC_FLAG(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_DIR64)

#pragma runtime_checks( "", off )
#pragma optimize( "", off )
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) {
    if (!pData) {
        pData->hMod = (HINSTANCE)0x404040;
        return;
    }

    uint8_t* pBase = pData->pbase;
    IMAGE_OPTIONAL_HEADER64* pOpt = &((IMAGE_NT_HEADERS*)(pBase + ((IMAGE_DOS_HEADER*)((uintptr_t)pBase))->e_lfanew))->OptionalHeader;

    f_LoadLibraryA _LoadLibraryA = pData->pLoadLibraryA;
    f_GetProcAddress _GetProcAddress = pData->pGetProcAddress;
    f_RtlAddFunctionTable _RtlAddFunctionTable = pData->pRtlAddFunctionTable;
    f_DLL_ENTRY_POINT _DllMain = (f_DLL_ENTRY_POINT)(pBase + pOpt->AddressOfEntryPoint);

    uint8_t* LocationDelta = pBase - pOpt->ImageBase;
    if (LocationDelta) {
        if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
            IMAGE_BASE_RELOCATION* pRelocData = (IMAGE_BASE_RELOCATION*)(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
            const IMAGE_BASE_RELOCATION* pRelocEnd = (IMAGE_BASE_RELOCATION*)((uintptr_t)(pRelocData) + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);
            while (pRelocData < pRelocEnd && pRelocData->SizeOfBlock) {
                uint32_t AmountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pRelativeInfo = (WORD*)(pRelocData + 1);

                for (uint32_t i = 0; i != AmountOfEntries; ++i, ++pRelativeInfo) {
                    if (RELOC_FLAG(*pRelativeInfo)) {
                        uintptr_t* pPatch = (UINT_PTR*)(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
                        *pPatch += (uintptr_t)(LocationDelta);
                    }
                }
                pRelocData = (IMAGE_BASE_RELOCATION*)((BYTE*)(pRelocData) + pRelocData->SizeOfBlock);
            }
        }
    }

    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        IMAGE_IMPORT_DESCRIPTOR* pImportDescr = (IMAGE_IMPORT_DESCRIPTOR*)(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (pImportDescr->Name) {
            char* szMod = (char*)(pBase + pImportDescr->Name);
            HINSTANCE hDll = _LoadLibraryA(szMod);

            ULONG_PTR* pThunkRef = (ULONG_PTR*)(pBase + pImportDescr->OriginalFirstThunk);
            ULONG_PTR* pFuncRef = (ULONG_PTR*)(pBase + pImportDescr->FirstThunk);

            if (!pThunkRef)
                pThunkRef = pFuncRef;

            for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {
                if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {
                    *pFuncRef = (ULONG_PTR)_GetProcAddress(hDll, (char*)(*pThunkRef & 0xFFFF));
                }
                else {
                    IMAGE_IMPORT_BY_NAME* pImport = (IMAGE_IMPORT_BY_NAME*)(pBase + (*pThunkRef));
                    *pFuncRef = (ULONG_PTR)_GetProcAddress(hDll, pImport->Name);
                }
            }
            ++pImportDescr;
        }
    }

    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
        IMAGE_TLS_DIRECTORY* pTLS = (IMAGE_TLS_DIRECTORY*)(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
        PIMAGE_TLS_CALLBACK* pCallback = (PIMAGE_TLS_CALLBACK*)(pTLS->AddressOfCallBacks);
        for (; pCallback && *pCallback; ++pCallback)
            (*pCallback)(pBase, DLL_PROCESS_ATTACH, NULL);
    }

    bool ExceptionSupportFailed = false;

    IMAGE_DATA_DIRECTORY excep = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (excep.Size) {
        if (!_RtlAddFunctionTable((PRUNTIME_FUNCTION)(pBase + excep.VirtualAddress), excep.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY), (DWORD64)pBase)) {
            ExceptionSupportFailed = true;
        }
    }

    _DllMain(pBase, DLL_PROCESS_ATTACH, NULL);

    if (ExceptionSupportFailed) {
        pData->hMod = (HINSTANCE) (0x505050);
    }
    else {
        pData->hMod = (HINSTANCE) (pBase);
    }
}
