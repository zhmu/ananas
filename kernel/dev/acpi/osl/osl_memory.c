#include <ananas/list.h>
#include <ananas/vm.h>
#include <ananas/mm.h>
#include <ananas/kmem.h>
#include <ananas/lib.h>
#include <machine/param.h>
#include <machine/vm.h>
#include "../acpica/acpi.h"

#define ACPI_DEBUG(...)

/*
 * ACPI likes to (re)map things over and over again; cope with this by
 * refcounting such attempts.
 */
struct ACPI_MEMORY_MAPPING {
	addr_t amm_phys;
	void* amm_virt;
	size_t amm_length;
	refcount_t amm_refcount;
	LIST_FIELDS(struct ACPI_MEMORY_MAPPING);
};

LIST_DEFINE(ACPI_MEMORY_MAPPINGS, struct ACPI_MEMORY_MAPPING);

static struct ACPI_MEMORY_MAPPINGS acpi_mappings;

void*
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
	ACPI_DEBUG("AcpiOsMapMemory(): pa=%p length=%d -> ", PhysicalAddress, Length);

	/*
	 * See if this thing is already mapped; if so, we just increase the refcount.
	 */
	int offset = PhysicalAddress & (PAGE_SIZE - 1);
	addr_t pa = PhysicalAddress - offset;
	size_t size = (Length + offset + PAGE_SIZE - 1) / PAGE_SIZE;
	LIST_FOREACH(&acpi_mappings, amm, struct ACPI_MEMORY_MAPPING) {
		if (amm->amm_phys < pa)
			continue;
		if (amm->amm_phys + amm->amm_length > pa + size)
			continue;
		if (pa + size > amm->amm_phys + amm->amm_length)
			continue;

		void* va = (void*)((addr_t)amm->amm_virt + offset);
		ACPI_DEBUG("FOUND va %p, pa %p..%p matches %p..%p, reffing (to %d), returning %p\n",
		 amm->amm_virt,
		 pa, pa + size * PAGE_SIZE,
		 amm->amm_phys, amm->amm_phys + amm->amm_length,
		 amm->amm_refcount + 1,
		 va);

		amm->amm_refcount++;
		return va;
	}

	ACPI_DEBUG("NEW pa=%p size=%d for phys=%p len=%p", pa, size, PhysicalAddress, Length);

	/* No luck; need to make a new mapping */
	struct ACPI_MEMORY_MAPPING* amm = kmalloc(sizeof *amm);
	amm->amm_phys = pa;
	amm->amm_virt = kmem_map(pa, size * PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_FORCEMAP);
	amm->amm_length = size;
	amm->amm_refcount = 1;
	if (amm->amm_virt == NULL) {
		ACPI_DEBUG(": FAILURE\n");
		kfree(amm);
		return NULL;
	}
	LIST_APPEND(&acpi_mappings, amm);

	void* va = (void*)((addr_t)amm->amm_virt + offset);
	ACPI_DEBUG(": %p\n", va);
	return va;
}

void
AcpiOsUnmapMemory(void* LogicalAddress, ACPI_SIZE Length)
{
	ACPI_DEBUG("AcpiOsUnmapMemory(): unmapping %p -> ", LogicalAddress);

	/* Locate the mapping; they are refcounted, so we may not need to remove it after all */
	int offset = (addr_t)LogicalAddress & (PAGE_SIZE - 1);
	addr_t la = (addr_t)LogicalAddress - offset;
	size_t size = (Length + offset + PAGE_SIZE - 1) / PAGE_SIZE;
	LIST_FOREACH(&acpi_mappings, amm, struct ACPI_MEMORY_MAPPING) {
		if ((addr_t)amm->amm_virt < la)
			continue;
		if (la + size > (addr_t)amm->amm_virt + amm->amm_length)
			continue;

		ACPI_DEBUG(" pa %p..%p matches %p..%p, dereffing",
		 LogicalAddress, (addr_t)LogicalAddress + Length, amm->amm_virt, amm->amm_virt + amm->amm_length * PAGE_SIZE);

		if (--amm->amm_refcount > 0) {
			ACPI_DEBUG("\n");
			return;
		}

		ACPI_DEBUG(" - throwing it away!\n");
		kmem_unmap(amm->amm_virt, amm->amm_length * PAGE_SIZE);

		LIST_REMOVE(&acpi_mappings, amm);
		kfree(amm);
		return;
	}
	panic("AcpiOsUnmapMemory(): mapping %p..%p not found", LogicalAddress, (addr_t)LogicalAddress + Length);
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
