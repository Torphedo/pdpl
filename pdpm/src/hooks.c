#include <stdio.h>

#include "hooks.h"

BOOL hook_ReadFile(HANDLE hFile, LPVOID buffer, uint32_t bytes_to_read, LPDWORD bytes_read_ptr, LPOVERLAPPED overlapped)
{
    printf("Intercepted call to ReadFile(), proceeding as normal.\n");
    return TRUE;
    return ReadFile(hFile, buffer, bytes_to_read, bytes_read_ptr, overlapped);
}
