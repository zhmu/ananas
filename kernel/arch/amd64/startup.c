#include "types.h"
#include "machine/bootinfo.h"
#include "vm.h"
#include "mm.h"
#include "lib.h"

/* Pointer to the next available page */
void* avail;

/* Pointer to the page directory level 4 */
uint64_t* pml4;

void*
bootstrap_get_page()
{
	void* ptr = avail;
	memset(ptr, 0, PAGE_SIZE);
	avail += PAGE_SIZE;
	return ptr;
}

void
md_startup(struct BOOTINFO* bi)
{
	/*
	 * This function will be called by the trampoline code, and hands us a bootinfo
	 * structure which contains the E820 memory map. We use this data to bootstrap
	 * the VM and initialize the memory manager.
	 */
	if (bi->bi_ident != BI_IDENT)
		panic("going nowhere without my bootinfo!");

	/*
	 * The loader tells us how large the kernel is; we use pages directly after
	 * the kernel.
	 */
	avail = (void*)bi->bi_kern_end;
	pml4 = (uint64_t*)bootstrap_get_page();

	/* First of all, map the kernel; we won't get far without it */
	void *__entry, *__end;
	addr_t from = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t   to = (addr_t)&__end;
	to += PAGE_SIZE - to % PAGE_SIZE;
	
	vm_mapto(from, from & 0x0fffffff /* HACK */, 32);

	/* Map the bootinfo block */
	vm_map((addr_t)bi, 1);
	vm_map((addr_t)bi->bi_e820_addr, 1);

	__asm(
		/* Set CR3 to the physical address of the page directory */
		"cli\n"
		"cli\n"
		"cli\n"
		"movq %%rax, %%cr3\n"
	: : "a" ((addr_t)pml4 & 0x0fffffff /* HACK */));

	#define SMAP_TYPE_MEMORY   0x01
	#define SMAP_TYPE_RESERVED 0x02
	#define SMAP_TYPE_RECLAIM  0x03
	#define SMAP_TYPE_NVS      0x04
	#define SMAP_TYPE_FINAL    0xFFFFFFFF /* added by the bootloader */

	struct SMAP_ENTRY {
		uint32_t base_lo, base_hi;
		uint32_t len_lo, len_hi;
		uint32_t type;
	} __attribute__((packed));

	/* Walk the memory map, and add each item one by one */
	struct SMAP_ENTRY* sme;
	for (sme = (struct SMAP_ENTRY*)(addr_t)bi->bi_e820_addr; sme->type != SMAP_TYPE_FINAL; sme++) {
		/* Ignore any memory that is not specifically available */
		if (sme->type != SMAP_TYPE_MEMORY)
			continue;

		addr_t base = (addr_t)sme->base_hi << 32 | sme->base_lo;
		size_t len = (size_t)sme->len_hi << 32 | sme->len_lo;

		base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		len  &= ~(PAGE_SIZE - 1);
		mm_zone_add(base, len);
	}

	mi_startup();
}

/* vim:set ts=2 sw=2: */
