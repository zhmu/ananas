#include "types.h"

void
vm_map(addr_t addr, size_t num_pages)
{
}

void*
vm_map_device(addr_t addr, size_t len)
{
	len = (len + PAGE_SIZE - 1) / PAGE_SIZE;
	vm_map(addr, len);
	return (void*)addr;
}

/* vim:set ts=2 sw=2: */
