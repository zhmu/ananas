#include <link.h>

#pragma weak dl_iterate_phdr
int
dl_iterate_phdr(int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data)
{
	/* to be implemented by the dynamic linker - TODO: implement this for static programs */
	return 0;
}
