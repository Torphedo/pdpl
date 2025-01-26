#include "dll.h"
#include <stdlib.h>

#include <Windows.h>

#include <common/path.h>
#include <common/file.h>
#include <common/logging.h>

#include "process.h"

typedef struct {
	uint32_t by_ordinal : 1; // Top bit is a bool flag
	uint32_t pad : 31; // Empty until the 32-bit boundary
	// Which one is used depends on flag
	union {
		uint16_t ordinal : 16; // Low 16 bits are ordinal value
		uint32_t name_rva : 31; // Low 31 bits are name RVA
	};
}import_lookup;

typedef struct BASE_RELOCATION_ENTRY {
    USHORT Offset : 12;
    USHORT Type : 4;
}BASE_RELOCATION_ENTRY;

static inline uint16_t lookup_ordinal(uint64_t lookup) {
	return (uint16_t)(lookup & 0xFFFF);
}
static inline uint32_t lookup_name_rva(uint64_t lookup) {
	return (uint32_t)(lookup & 0xFFFFFFFF);
}
static inline bool is_ordinal(uint64_t lookup) {
	return lookup & IMAGE_ORDINAL_FLAG;
}

/* Small shellcode function to call DllMain(). The address here should
 * be replaced with the address of DllMain() at runtime.
void dll_main_thunk(void* base_addr) {
    dll_entry dll_main = (dll_entry)0x00000001DB600000;
    dll_main((HINSTANCE)base_addr, DLL_PROCESS_ATTACH, 0);
}
*/
uint8_t dll_main_thunk[] = {
	// movabs rax, 0x0000001DB600000 ; Load function addr
	0x48, 0xB8, 0x00, 0x00, 0x60, 0xDB, 0x01, 0x00, 0x00, 0x00,

	// mov edx 0x00000001 ; [DLL_PROCESS_ATTACH]
	0xBA, 0x01, 0x00, 0x00, 0x00,

	// xor r8d, r8d ; Zero out last argument
	0x45, 0x31, 0xC0,

	// rex.W jump rax ; Jump to function address
	0x48, 0xFF, 0xE0 
};

IMAGE_SECTION_HEADER get_section_header(const char* name, uint8_t* dll_data) {
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)dll_data;
	IMAGE_NT_HEADERS* nt_header =  (IMAGE_NT_HEADERS*)(dll_data + dos_header->e_lfanew);

	// Get section headers
	uint32_t section_header_rva = dos_header->e_lfanew + sizeof(*nt_header);
	IMAGE_SECTION_HEADER* headers = (IMAGE_SECTION_HEADER*)(dll_data + section_header_rva);

	// Search through the sections for our target
	for (uint32_t i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
		char* cur_name = (char*)headers[i].Name;
		if (strncmp(name, cur_name, strlen(name)) == 0) {
			return headers[i];
		}
	}

	// Oh well.
	IMAGE_SECTION_HEADER failure = {0};
	return failure;
}

bool call_dll_main_remote(uint32_t pid, dll_entry dll_main, void* base_addr) {
	// We need to overwrite the address of DllMain() in our assembled thunk.
	// The immediate address data starts at the third byte (index 2)
	uintptr_t* dll_main_addr = (uintptr_t*)&dll_main_thunk[2];
	*dll_main_addr = (uintptr_t)dll_main; // Write the address right to the assembled code

	// Try to open the target process with required permissions
	uint32_t permissions = PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD;
	HANDLE remote = OpenProcess(permissions, false, pid);
	if (remote == INVALID_HANDLE_VALUE) {
		LOG_MSG(error, "Failed to open target process %d\n", pid);
		return false;
	}

	uint8_t* shellcode = VirtualAllocEx(remote, NULL, sizeof(dll_main_thunk), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (shellcode == NULL) {
		LOG_MSG(error, "Failed to allocate memory for remote thunk\n");
		CloseHandle(remote);
		return false;
	}

	// TODO: Handle more errors here
	// Copy and execute the assembled function in the remote process
	WriteProcessMemory(remote, shellcode, dll_main_thunk, sizeof(dll_main_thunk), NULL);

	CreateRemoteThread(remote, NULL, 0, (LPTHREAD_START_ROUTINE)shellcode, base_addr, 0, NULL);
	return true;
}

/// Load each PE section to its correct virtual address in [image_buffer]
/// \param dll_file Buffer containing the contents of a DLL file
/// \param image_buffer Empty buffer of [OptionalHeader.SizeOfImage] bytes
void pe_load_sections(uint8_t* dll_file, uint8_t* image_buffer) {
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)dll_file;
	IMAGE_NT_HEADERS* nt_header = (IMAGE_NT_HEADERS*)(dll_file + dos_header->e_lfanew);
	uint32_t image_size = nt_header->OptionalHeader.SizeOfImage;
	uint64_t image_base = nt_header->OptionalHeader.ImageBase;

	// Copy file headers to the new buffer
	uint32_t section_headers_size = (nt_header->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER));
	uint32_t headers_size = dos_header->e_lfanew + sizeof(*nt_header) + section_headers_size;
	memcpy(image_buffer, dll_file, headers_size);

	// Loop through all sections
	IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt_header);
	for (uint32_t i = 0; i < nt_header->FileHeader.NumberOfSections; i++) {
		uint64_t section_va = sections[i].VirtualAddress;
		uint64_t section_size = sections[i].SizeOfRawData;
		uint32_t flags = sections[i].Characteristics;

		if (section_size == 0) {
			if ((flags & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0) {
				section_size = nt_header->OptionalHeader.SizeOfInitializedData;
			}
			if ((flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0) {
				section_size = nt_header->OptionalHeader.SizeOfUninitializedData;
			}
		}

		// LOG_MSG(debug, "%s VA = 0x%x [size 0x%x]\n", (char*)(&sections[i].Name), sections[i].VirtualAddress, section_size);

		// Copy section to appropriate address
		if (section_size > 0) {
			memcpy(image_buffer + section_va, dll_file + sections[i].PointerToRawData, section_size);
		}
	}
}

bool dll_inject_memory_file(uint32_t pid, const char* path) {
	uint8_t* dll = file_load(path);
	if (dll == NULL) {
		LOG_MSG(error, "Failed to read DLL.\n");
		return false;
	}
	bool result = dll_inject_memory(pid, dll);
	free(dll);
	return result;
}

bool dll_inject_memory(uint32_t pid, uint8_t* dll_data) {
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)dll_data;
	IMAGE_NT_HEADERS* nt_header = (IMAGE_NT_HEADERS*)(dll_data + dos_header->e_lfanew);
	uint32_t image_size = nt_header->OptionalHeader.SizeOfImage;
	uint64_t image_base = nt_header->OptionalHeader.ImageBase;

	uint8_t* new_image = calloc(1, image_size);
	if (new_image == NULL) {
		LOG_MSG(error, "Failed to allocate scratch buffer to load the DLL.\n");
		return false;
	}

	// Move each section to its correct virtual address in a new buffer
	pe_load_sections(dll_data, new_image);

	IMAGE_SECTION_HEADER import_section = get_section_header(".idata", new_image);
	LOG_MSG(debug, ".idata offset = 0x%lx, VA = 0x%x\n", import_section.PointerToRawData, import_section.VirtualAddress);

	IMAGE_IMPORT_DESCRIPTOR* import_dir = (IMAGE_IMPORT_DESCRIPTOR*)(new_image + import_section.VirtualAddress);

    // Try to open the target process with required permissions
    uint32_t permissions = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD;
    HANDLE remote = OpenProcess(permissions, false, pid);
    if (remote == INVALID_HANDLE_VALUE) {
        LOG_MSG(error, "Failed to open target process %d\n", pid);
        return false;
    }

	// Loop until we hit the null entry
	while (import_dir->Name > 0) {
		char* module_name = (char*)(new_image + import_dir->Name);
        // Make sure we and the target both have the DLL loaded
		LoadLibrary(module_name);
        remote_load_library(pid, module_name);

		uintptr_t remote_handle = (uintptr_t)get_remote_module(module_name, remote); // Address in remote process
		uintptr_t local_handle = (uintptr_t)GetModuleHandle(module_name); // Address in our process
		LOG_MSG(debug, "%s is at 0x%p\n", module_name, remote_handle);

		uint64_t* lookup = (uint64_t*)(new_image + import_dir->OriginalFirstThunk);

		// Loop through the array of name RVAs and/or ordinal imports
		uint32_t lookup_idx = 0;
		while (*lookup != 0) {
			const bool use_ordinal = is_ordinal(*lookup);
			const uint32_t func_name_rva = lookup_name_rva(*lookup);

			if (use_ordinal) {
                const uint16_t ordinal = lookup_ordinal(*lookup);
				LOG_MSG(info, "DLL imports %s@%d\n", module_name, ordinal);
				LOG_MSG(error, "Unimplemented: ordinal import\n");
			}
			else {
				IMAGE_IMPORT_BY_NAME* name_hint = (IMAGE_IMPORT_BY_NAME*)(new_image + func_name_rva);
				char* func_name = (char*)&name_hint->Name;
				IMAGE_THUNK_DATA* addresses = (IMAGE_THUNK_DATA*)(new_image + import_dir->FirstThunk);

				void* proc_addr = get_remote_proc_addr(module_name, func_name, remote);
				addresses[lookup_idx].u1.AddressOfData = (ULONGLONG)proc_addr;

				LOG_MSG(info, "DLL imports %s::%s\n", module_name, func_name);
			}
			// Go to next function import
			lookup = &lookup[1];
			lookup_idx++;
		}
		// Go to next DLL
		import_dir = &import_dir[1];
	}

    // Relocate image, to ensure that it will have correct addresses once it's in the target process
    IMAGE_BASE_RELOCATION* reloc_table = (IMAGE_BASE_RELOCATION*)((uintptr_t)new_image + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

    while (reloc_table->SizeOfBlock > 0) {
        // BASE_RELOCATION_ENTRY is 2 bytes, so the size (minus header) over 2 is our entry count
        // (not using sizeof(BASE_RELOCATION_ENTRY) because some compilers might add padding or something on bitfields)
        uint32_t entries_count = (reloc_table->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        // This VirtualAddress member tells us the offset of the relocation table from the image base.
        BASE_RELOCATION_ENTRY* entries = (BASE_RELOCATION_ENTRY*)((uintptr_t)reloc_table + sizeof(*reloc_table));

        for (uint32_t i = 0; i < entries_count; i++) {
            if (entries[i].Offset) {
                // Get the address of the pointer we need to relocate, then add our image base delta.
                uintptr_t* patched_address = (uintptr_t*)((uintptr_t)new_image + reloc_table->VirtualAddress + entries[i].Offset);
                // *patched_address += (uintptr_t)new_image;
            }
        }

        // Go to the next relocation table and repeat. There is one table per 4KiB page.
        reloc_table = (IMAGE_BASE_RELOCATION*)((uintptr_t)reloc_table + reloc_table->SizeOfBlock);
    }


    uint8_t* remote_image = VirtualAllocEx(remote, (uint8_t*)image_base, image_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (remote_image == NULL) {
		LOG_MSG(error, "Couldn't allocate 0x%lx bytes for remote image\n", image_size);
		CloseHandle(remote);
		return false;
	}

	uintptr_t reloc_bias = (uintptr_t)remote_image - (uintptr_t)image_base;
	if (reloc_bias != 0) {
		LOG_MSG(debug, "reloc bias = 0x%x\n", reloc_bias);
	}

	// Write image to remote process
	WriteProcessMemory(remote, remote_image, new_image, image_size, NULL);
    CloseHandle(remote);

	// Get TLS data
	uint32_t tls_rva = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress;
	if (tls_rva != 0) {
		IMAGE_TLS_DIRECTORY* tls = (IMAGE_TLS_DIRECTORY*)(new_image + tls_rva);
		PIMAGE_TLS_CALLBACK* tls_callbacks = (PIMAGE_TLS_CALLBACK*)((tls->AddressOfCallBacks - (uintptr_t)remote_image) + new_image);
		while (*tls_callbacks != NULL) {
			// Call TLS callback and move on to the next entry
			call_dll_main_remote(pid, (dll_entry)*tls_callbacks, remote_image);
			tls_callbacks++;
		}
	}

	dll_entry entry_point = (dll_entry)(remote_image + nt_header->OptionalHeader.AddressOfEntryPoint);
	call_dll_main_remote(pid, entry_point, remote_image);
	free(new_image);
	return true;
}

bool remote_load_library_existing(HANDLE remote, const char* dll_path) {
    // Allocate & write path in remote process
    uint32_t size = strnlen(dll_path, MAX_PATH) + 1;
    char* remote_path = VirtualAllocEx(remote, NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (remote_path == NULL) {
        LOG_MSG(error, "Failed to allocate %d bytes for remote filepath \"%s\"\n", size, dll_path);
        CloseHandle(remote);
        return false;
    }

    WriteProcessMemory(remote, remote_path, dll_path, size, NULL);

    // Get &LoadLibraryA()
    void* load_library = get_remote_proc_addr("KERNEL32.dll", "LoadLibraryA", remote);

    // Start execution.
    HANDLE remote_thread = CreateRemoteThread(remote, NULL, 0, load_library, remote_path, 0, NULL);
    if (remote_thread == NULL) {
        LOG_MSG(error, "Failed to start remote thread.\n");
        VirtualFreeEx(remote, remote_path, 0, MEM_RELEASE); // Free remote memory
        CloseHandle(remote);
        return false;
    }

    WaitForSingleObject(remote_thread, INFINITE);
    VirtualFreeEx(remote, remote_path, 0, MEM_RELEASE);

    CloseHandle(remote);
    return true;
}

bool remote_load_library(uint32_t pid, const char* dll_path) {
	// Try to open the target process with required permissions
    DWORD access = PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION;
	HANDLE remote = OpenProcess(access, false, pid);
	if (remote == NULL) {
		LOG_MSG(error, "Failed to open target process %d\n", pid);
		return false;
	}

    return remote_load_library_existing(remote, dll_path);
}