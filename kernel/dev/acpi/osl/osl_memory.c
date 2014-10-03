#include <ananas/vm.h>
#include <ananas/mm.h>
#include <machine/param.h>
#include <machine/vm.h>
#include "../acpica/acpi.h"

/* XXX Kludge for the terrible hacks up ahead */
#ifdef __i386__
#define TEMP_MAPPING_RANGE 0xeff00000
#else
#error Fix me!
#endif

void*
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
	addr_t delta = PhysicalAddress & (PAGE_SIZE - 1);
	addr_t phys_addr = PhysicalAddress - delta;
	int num_pages = (Length + delta + (PAGE_SIZE - 1)) / PAGE_SIZE;
	int flags = VM_FLAG_READ | VM_FLAG_WRITE;
	addr_t virt_addr;

	/*
	 * XXX Kludge: if we have already mapped this page, it'll likely be in use
	 *     by the kernel itself - prevent frees from memory we care about by
	 *     mapping to some other range instead. We should tell the kernel about
	 *     this so it will never use those pages at all :-/
	 */
	if (md_get_mapping(NULL, PTOKV(phys_addr), 0, NULL)) {
		virt_addr = TEMP_MAPPING_RANGE;
		addr_t cur_mapping;
		int mapped_ok = 0;
		while (md_get_mapping(NULL, virt_addr, VM_FLAG_READ, &cur_mapping)) {
			if (cur_mapping == phys_addr)
				mapped_ok++;
			else
				mapped_ok = 0;
			virt_addr += PAGE_SIZE;
		}
		if (mapped_ok == num_pages)
			return (void*)(virt_addr - PAGE_SIZE * mapped_ok);
		md_map_pages(NULL, virt_addr, phys_addr, num_pages, flags);
	} else {
		virt_addr = (addr_t)vm_map_kernel(phys_addr, num_pages, VM_FLAG_KERNEL | flags);
	}
	if (virt_addr == 0)
		return NULL;
	return (void*)(virt_addr + delta);
}

void
AcpiOsUnmapMemory(void* LogicalAddress, ACPI_SIZE Length)
{
	addr_t addr = (addr_t)LogicalAddress;
	addr_t delta = addr & (PAGE_SIZE - 1);
	vm_unmap_kernel(addr - delta, (Length + delta + (PAGE_SIZE - 1)) / PAGE_SIZE);
}

void*
AcpiOsAllocate(ACPI_SIZE Size)
{
	return kmalloc(Size);
}

void
AcpiOsFree(void* Memory)
{
	kfree(Memory);
}

/* vim:set ts=2 sw=2: */
