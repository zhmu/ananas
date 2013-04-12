#include <ananas/vm.h>
#include <ananas/mm.h>
#include <machine/param.h>
#include "../acpica/acpi.h"

void*
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
	addr_t delta = PhysicalAddress & (PAGE_SIZE - 1);
	addr_t addr = (addr_t)vm_map_kernel(PhysicalAddress - delta, (Length + delta + (PAGE_SIZE - 1)) / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);
	if (addr == 0)
		return NULL;
	return (void*)(addr + delta);
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
