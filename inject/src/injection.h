#pragma once

#include <stdbool.h>
#include <stdint.h>

uint32_t get_pid_by_name(LPCTSTR ProcessName);
void set_access_control(const char* ExecutableName, const char* AccessString);
bool dll_inject_remote(uint32_t process_id, const char* dll_path, uint64_t dll_path_size);
