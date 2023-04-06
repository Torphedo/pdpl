#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef HINSTANCE(*f_LoadLibraryA )(const char* lpLibFilename);
typedef FARPROC(*f_GetProcAddress)(HMODULE hModule, LPCSTR lpProcName);
typedef BOOL(*f_DLL_ENTRY_POINT)(void* hDll, DWORD dwReason, void* pReserved);
typedef BOOL(*f_RtlAddFunctionTable)(PRUNTIME_FUNCTION FunctionTable, DWORD EntryCount, DWORD64 BaseAddress);

typedef struct {
    f_LoadLibraryA pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
    f_RtlAddFunctionTable pRtlAddFunctionTable;
    uint8_t* pbase;
    HINSTANCE hMod;
    BOOL SEHSupport;
}MANUAL_MAPPING_DATA;
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData);
bool ManualMapDll(HANDLE process, bool ClearHeader, bool ClearNonNeededSections, bool AdjustProtections, bool SEHExceptionSupport);

uint32_t get_pid_by_name(const char* ProcessName);
