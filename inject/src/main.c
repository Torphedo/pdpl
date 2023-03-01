#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#include <windows.h>
#include <TlHelp32.h>

// Setting DLL access controls
#include <Aclapi.h>

const wchar_t* DLLFile = L"payload.dll";

void set_access_control(const wchar_t* ExecutableName, const wchar_t* AccessString);

bool dll_inject_remote(uint32_t process_id, const wchar_t* dll_path, const uint64_t dll_path_size);

static DWORD get_pid_by_name(LPCTSTR ProcessName)
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

int main(int argc, char** argv, char** envp)
{
	uint32_t ProcessID = get_pid_by_name("PDUWP.exe");

	// Enable VT100
	DWORD ConsoleMode;
	GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &ConsoleMode);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ConsoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	wchar_t run_path[MAX_PATH] = { 0 }; // Wide character string large enough to hold any path
	GetCurrentDirectoryW(MAX_PATH, run_path);

	uint64_t total_size = sizeof(*run_path) * (wcslen(run_path) + sizeof(*run_path) + wcslen(DLLFile) + 1);
	wchar_t* dll_path = (wchar_t*) calloc(1, total_size);
	if (dll_path == NULL)
	{
		printf("main(): Failed to allocate %lli bytes for path to payload DLL.\n", total_size);
		return EXIT_FAILURE;
	}

	// Add a backslash, then the DLL file we're loading from the same folder.
#if _MSC_VER && !__INTEL_COMPILER
	wcscpy_s(dll_path, total_size, run_path);
	wcscat_s(dll_path, total_size, L"\\");
	wcscat_s(dll_path, total_size, DLLFile);
#elif
	wcscpy(dll_path, run_path); // We don't check size because total length is always longer.
	wcscat(dll_path, L"\\");    // These should fit because we allocated enough space
	wcscat(dll_path, DLLFile);
#endif

	set_access_control(dll_path, L"S-1-15-2-1");

	printf("\033[93mInjecting into remote process: ");
	if(!dll_inject_remote(ProcessID, dll_path, total_size))
	{
		free(dll_path);
		printf("\033[91mFailed\033[0m\n");
		system("pause");
		return EXIT_FAILURE;
	}
	free(dll_path);
	printf("\033[92mSuccess!\033[0m\n");

	return EXIT_SUCCESS;
}

void set_access_control(const wchar_t* ExecutableName, const wchar_t* AccessString)
{
	PSECURITY_DESCRIPTOR SecurityDescriptor = NULL;
	EXPLICIT_ACCESSW ExplicitAccess = { 0 };

	ACL* AccessControlCurrent = NULL;
	ACL* AccessControlNew = NULL;

	SECURITY_INFORMATION SecurityInfo = DACL_SECURITY_INFORMATION;
	PSID SecurityIdentifier = NULL;

	if(
		GetNamedSecurityInfoW(
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
		ConvertStringSidToSidW(AccessString, &SecurityIdentifier);
		if( SecurityIdentifier != NULL )
		{
			ExplicitAccess.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE | GENERIC_WRITE;
			ExplicitAccess.grfAccessMode = SET_ACCESS;
			ExplicitAccess.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
			ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
			ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
			ExplicitAccess.Trustee.ptstrName = (wchar_t*)(SecurityIdentifier);

			if(
				SetEntriesInAclW(
					1,
					&ExplicitAccess,
					AccessControlCurrent,
					&AccessControlNew
				) == ERROR_SUCCESS
				)
			{
				SetNamedSecurityInfoW(
					(wchar_t*)(ExecutableName),
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

bool dll_inject_remote(uint32_t process_id, const wchar_t* dll_path, const uint64_t dll_path_size)
{
	if( !process_id )
	{
		printf("Invalid Process ID: %d\n", process_id);
		return false;
	}
	if( GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES )
	{
		printf("The requested DLL \"%ws\" does not exist.\n", dll_path);
		return false;
	}

	set_access_control(dll_path, L"S-1-15-2-1");

	void* proc_LoadLibrary = (void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

	if(!proc_LoadLibrary)
	{
		printf("Unable to find LoadLibraryW procedure.\n");
		return false;
	}

	void* process = OpenProcess(PROCESS_ALL_ACCESS, false, process_id);
	if( process == NULL )
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
		// Explicitly wait for LoadLibraryW to complete before releasing memory
		// avoids causing a remote memory leak
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
