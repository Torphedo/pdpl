#include <stdint.h>
#include <stdbool.h>

void* get_remote_proc_addr(const char* module_name, const char* proc_name, uint32_t pid);
void* get_remote_module(const char* module_name, uint32_t pid);
bool enable_debug_privilege(bool bEnable);

