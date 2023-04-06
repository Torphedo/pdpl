#pragma once

#include <stdbool.h>
#include <stdint.h>

bool self_inject(HANDLE target);

uint32_t get_pid_by_name(const char* ProcessName);
