#include <ananas/vm.h>
#include <ananas/lib.h>

void*
vm_map_kernel(addr_t addr, size_t num_pages, int flags)
{
	/* XXX */
	return (void*)addr;
}

void
vm_unmap_kernel(addr_t addr, size_t num_pages)
{
	/* XXX */
}

void*
vm_map_device(addr_t addr, size_t len)
{
	panic("TODO: vm_map_device(): addr=%x, len=%x\n", addr, len);
	return NULL;
}

/* vim:set ts=2 sw=2: */
