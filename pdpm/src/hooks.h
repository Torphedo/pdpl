#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOMB
#include <Windows.h>

#include <stdint.h>
#include <stdbool.h>

bool setup_hooks();
bool hook_ReadFile(HANDLE hFile, LPVOID buffer, uint32_t bytes_to_read, LPDWORD bytes_read_ptr, LPOVERLAPPED overlapped);