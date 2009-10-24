#include "types.h"
#include "elf.h"
#include "param.h"
#include "trampoline.h"

#define PE_P		(1ULL <<  0)
#define PE_RW		(1ULL <<  1)
#define PE_US		(1ULL <<  2)
#define PE_PWT	(1ULL <<  3)
#define PE_PCD	(1ULL <<  4)
#define PE_A		(1ULL <<  5)
#define PE_D		(1ULL <<  6)
#define PE_PS		(1ULL <<  7)
#define PE_G		(1ULL <<  8)
#define PE_PAT	(1ULL << 12)
#define PE_NX		(1ULL << 63)

/* Points to the next available free page */
void* avail;

/* PML4 pointer */
uint64_t* pml4;

static void*
get_page(size_t num)
{
	void* ptr = avail;
	memset(ptr, 0, num * PAGE_SIZE);
	avail += (num * PAGE_SIZE);
	return ptr;
}

#define get_page_one() get_page(1)

void
vm_map(uint64_t dest, addr_t src, int num_pages)
{
	/*
	 * In amd64 mode with 2MB pages, dest is represented as:
	 *
	 * - sign extend            (bits 63-48): duplication of bit 47
	 * - page-map level4 table  (bits 47-39): index in the PML4 table
	 * - page-dir-pointer table (bits 38-30): index in the PML4's PDP table
	 * - page-dir offset table  (bits 29-21): index in the PDP's page-dir table
	 * - physical offset        (bits 20- 0): copied as-is
	 *
	 * XXX we currently ignore bits 48-63
	 */
	while(num_pages--) {
__asm("cli");
__asm("cli");
__asm("cli");
		if (pml4[(dest >> 39) & 0x1ff] == 0) {
			/* No entry there */
			pml4[(dest >> 39) & 0x1ff] = (uint64_t)((addr_t)get_page_one()) | PE_RW | PE_P;
		}
		uint64_t* pdpe = (uint64_t*)((addr_t)(pml4[(dest >> 39) & 0x1ff] & 0xfffff000));

		if (pdpe[(dest >> 30) & 0x1ff] == 0) {
			pdpe[(dest >> 30) & 0x1ff] = (uint64_t)((addr_t)get_page_one()) | PE_RW | PE_P;
		}
		uint64_t* pde = (uint64_t*)((addr_t)pdpe[(dest >> 30) & 0x1ff] & 0xfffff000);

		pde[(dest >> 21) & 0x1ff] = (uint64_t)(src | PE_PS | PE_RW | PE_P);

		dest += PAGE_SIZE; src += PAGE_SIZE;
	}
}

void
startup()
{
	/*
	 * Once this function is called, we assume to be run in 32 bit protected mode
	 * without any paging. We need to initialize a paging-table for amd64 operation,
	 * and relocate a kernel (note that we can't place it at the correct address, but
	 * this does not matter since the top-bits are sign-extended anyway).
	 *
	 * In order to simply things for us, we use 2MB pages (1GB pages would be even
	 * better, but early AMD64 systems don't support them...) - our responsibility
	 * is to have a kernel loaded and ready; the kernel is responsible for creating
	 * descriptors and paging tables it's happy with, ours only need to be
	 * sufficient for allowing to to launch.
	 */
	avail = (void*)AVAIL_ADDR;

	pml4 = (uint64_t*)get_page_one();

	/* Provide an identity mapping for our trampoline code */
	void *__entry, *__end;
	vm_map((uint64_t)ROUND_2MB((addr_t)&__entry), ROUND_2MB((addr_t)&__entry), 1 /* 2MB ought to be enough */);
//(&__end - &__entry) / 1024 * 1024 * 2

	/* Relocate the kernel */
	relocate_kernel();
}

/* vim:set ts=2 sw=2: */
