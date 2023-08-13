#pragma once
#include <stdbool.h>
#include <stdint.h>

uint32_t get_pid_by_name(const char* ProcessName);
bool self_inject(uint32_t process_id, LPTHREAD_START_ROUTINE entry_point, void* parameter);
