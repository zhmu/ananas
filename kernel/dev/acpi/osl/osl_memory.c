#include <ananas/vm.h>
#include <ananas/mm.h>
#include <machine/param.h>
#include <machine/vm.h>
#include "../acpica/acpi.h"

/* XXX Kludge for the terrible hacks up ahead */
#ifdef __i386__
#define TEMP_MAPPING_RANGE 0xeff00000

extern uint32_t* kernel_pd;
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
	if (md_get_mapping(kernel_pd, PTOKV(phys_addr), 0, NULL)) {
		virt_addr = TEMP_MAPPING_RANGE;
		addr_t cur_mapping;
		while (md_get_mapping(kernel_pd, virt_addr, VM_FLAG_READ, &cur_mapping)) {
			if (cur_mapping == phys_addr) {
				return (void*)(virt_addr + delta);
			}
			virt_addr += PAGE_SIZE;
		}
		md_map_pages(kernel_pd, virt_addr, phys_addr, num_pages, flags);
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
