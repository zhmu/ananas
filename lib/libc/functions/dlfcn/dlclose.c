#include <dlfcn.h>

#pragma weak dlclose

int dlclose(void* handle) { return -1; }
