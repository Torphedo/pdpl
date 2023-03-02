#include <stdio.h>
#include <Aclapi.h>
#include <sddl.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <TlHelp32.h>

#include "injection.h"

/*
 * get_pid_by_name(): Taken from Skill Editor (Torphedo)
*/
uint32_t get_pid_by_name(LPCTSTR ProcessName)
{
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

/*
 * set_access_control(): Adapted from its C++ equivalent in UWPDUmper (Wunkolo)
 * https://github.com/Wunkolo/UWPDumper/blob/master/UWPInjector/source/main.cpp#L440-L503
 * Calling this doesn't actually seem to be necessary for injecting into a UWP app.
 * Maybe this makes injection more reliable in edge cases or allows files to be dumped.
 */
void set_access_control(const char* ExecutableName, const char* AccessString)
{
    PSECURITY_DESCRIPTOR SecurityDescriptor = NULL;
    EXPLICIT_ACCESS ExplicitAccess = { 0 };

    ACL* AccessControlCurrent = NULL;
    ACL* AccessControlNew = NULL;

    SECURITY_INFORMATION SecurityInfo = DACL_SECURITY_INFORMATION;
    PSID SecurityIdentifier = NULL;

    if(
            GetNamedSecurityInfo(
                    ExecutableName,
                    SE_FILE_OBJECT,
                    DACL_SECURITY_INFORMATION,
                    NULL,
                    NULL,
                    &AccessControlCurrent,
                    NULL,
                    &SecurityDescriptor
            ) == ERROR_SUCCESS
            )
    {
        ConvertStringSidToSid(AccessString, &SecurityIdentifier);
        if( SecurityIdentifier != NULL )
        {
            ExplicitAccess.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE | GENERIC_WRITE;
            ExplicitAccess.grfAccessMode = SET_ACCESS;
            ExplicitAccess.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
            ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            ExplicitAccess.Trustee.ptstrName = (SecurityIdentifier);

            if(
                    SetEntriesInAcl(
                            1,
                            &ExplicitAccess,
                            AccessControlCurrent,
                            &AccessControlNew
                    ) == ERROR_SUCCESS
                    )
            {
                SetNamedSecurityInfo(
                        (char*)ExecutableName,
                        SE_FILE_OBJECT,
                        SecurityInfo,
                        NULL,
                        NULL,
                        AccessControlNew,
                        NULL
                );
            }
        }
    }
    if(SecurityDescriptor)
    {
        LocalFree((HLOCAL)(SecurityDescriptor));
    }
    if(AccessControlNew)
    {
        LocalFree((HLOCAL)(AccessControlNew));
    }
}

/*
 * dll_inject_remote(): Manual map DLL injector, adapted from C++ code in UWPDumper (Wunkolo)
 * https://github.com/Wunkolo/UWPDumper/blob/master/UWPInjector/source/main.cpp#L440-L503
 */
bool dll_inject_remote(uint32_t process_id, const char* dll_path, uint64_t dll_path_size)
{
    if(!process_id)
    {
        printf("Invalid Process ID: %d\n", process_id);
        return false;
    }
    if(GetFileAttributes(dll_path) == INVALID_FILE_ATTRIBUTES)
    {
        printf("The requested DLL \"%s\" does not exist.\n", dll_path);
        return false;
    }

    set_access_control(dll_path, "S-1-15-2-1");

    void* proc_LoadLibrary = (void*)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");

    if(!proc_LoadLibrary)
    {
        printf("Unable to find LoadLibraryA procedure.\n");
        return false;
    }

    void* process = OpenProcess(PROCESS_ALL_ACCESS, false, process_id);
    if(process == NULL)
    {
        printf("Unable to open process ID %d for writing\n", process_id);
        return false;
    }
    void* remote_dll_path = (void*)VirtualAllocEx(process, NULL, dll_path_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if(remote_dll_path == NULL)
    {
        printf("Unable to remotely allocate memory.\n");
        CloseHandle(process);
        return false;
    }

    SIZE_T size_written = 0;
    uint32_t result = WriteProcessMemory(process, remote_dll_path, dll_path, dll_path_size, &size_written);

    if(result == 0)
    {
        printf("Unable to write process memory.\n");
        CloseHandle(process);
        return false;
    }

    if(size_written != dll_path_size)
    {
        printf("Failed to write remote DLL path name.\n");
        CloseHandle(process);
        return false;
    }

    void* remote_thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)(proc_LoadLibrary),remote_dll_path, 0, NULL);

    // Wait for remote thread to finish
    if(remote_thread)
    {
        // Explicitly wait for LoadLibrary to complete before releasing memory to avoid causing a remote memory leak
        WaitForSingleObject(remote_thread, INFINITE);
        CloseHandle(remote_thread);
    }
    else
    {
        // Failed to create thread
        printf("Unable to create remote thread.\n");
    }

    VirtualFreeEx(process, remote_dll_path, 0, MEM_RELEASE);
    CloseHandle(process);
    return true;
}
