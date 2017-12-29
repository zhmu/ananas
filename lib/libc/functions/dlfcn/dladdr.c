#include <dlfcn.h>

#pragma weak dladdr

int
dladdr(const void* addr, Dl_info* info)
{
	return 0;
}
