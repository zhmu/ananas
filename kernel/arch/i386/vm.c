#include "i386/types.h"
#include "i386/vm.h"
#include "param.h"

/* XXX This is a hack - a terrible hack - and must be nuked soon */
static uint8_t temp_pt_entry[PAGE_SIZE];

void*
vm_map_device(addr_t addr, size_t len)
{
	void* result = (void*)addr;
	/*KASSERT(addr & PAGE_SIZE == 0, "vm_map_device: unaligned address 0x%x", addr);*/

	while (len > 0) {
		uint32_t pd_entrynum = addr >> 22;
		if (pagedir[pd_entrynum] == 0) {
			/*
			 * This page isn't yet mapped, so we need to allocate a new page entry
			 * where we can map the pointer.
			 *
			 * XXX until a memory allocator is created; recycle the same page...
			 *     since we map only a single temporary page... as long as we
			 *     only map a console, this is fine anyway XXX
			 */
			void* new_entry = (void*)&temp_pt_entry;
			pagedir[pd_entrynum] = ((addr_t)new_entry - KERNBASE) | PDE_P | PTE_RW;
		}

		uint32_t* pt = (uint32_t*)(pagedir[pd_entrynum] & ~(PAGE_SIZE - 1) | KERNBASE);
		pt[(((addr >> 12) & ((1 << 10) - 1)))] = addr | PTE_P | PTE_RW;

		if (len < PAGE_SIZE)
			break;
		len -= PAGE_SIZE;
	}

	return (void*)result;
}

/* vim:set ts=2 sw=2: */
