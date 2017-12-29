#include <dlfcn.h>
#include <stdlib.h>

#pragma weak dlopen

void* dlopen(const char* file, int mode)
{
	return NULL;
}
