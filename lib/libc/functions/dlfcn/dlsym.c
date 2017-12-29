#include <dlfcn.h>
#include <stdlib.h>

#pragma weak dlsym

void* dlsym(void* handle, const char* name)
{
	return NULL;
}
