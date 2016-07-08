#include "amd64.h"
#include "io.h"
#include "lib.h"

#define PAGE_SIZE 4096
#define PE_P	(1ULL << 0)
#define PE_RW	(1ULL << 1)
#define PE_PS	(1ULL << 7)

void
amd64_exec(uint64_t entry, struct BOOTINFO* bootinfo)
{
	/*
	 * Cook up some location to store the page table entries; we just need
	 * 3 pages for this, the only thing that matters is that they are
	 * page-aligned.
	 *
	 * We'll assume the entire base memory is available, so let's just steal a
	 * spot from there; we know the Ananas kernel will use memory after the
	 * kernel to store the page tables and other data, so anything in base
	 * memory ought to work (as long as we don't overwrite things like the
	 * stack)
 	 */
	addr_t base = 0x10000; /* 1000:0 */;

	/*
	 * In x86_64 (amd64) mode, paging is mandatory - this means we should
	 * immediately map the kernel's virtual address space to the physical
	 * location where we loaded it.
	 *
 	 * As inspired by FreeBSD's bootloader, we can take a rather convenient
	 * shortcut here: ignoring the top 32 bits, we can map everything as
	 * follows:
	 *
	 *   Virtual                       Physical
	 *   0x00000000-0x3fffffff  /  \
	 *   0x40000000-0x7fffffff /____\  0x00000000-0x3fffffff 
	 *   0x80000000-0xbfffffff \    /
	 *   0xc0000000-0xffffffff  \  /
	 *
	 * The kernel lives at 0xffffffff80100000, which conveniently matches up
	 * with 1MB in physical memory (where we should have placed it).
	 *
	 * We use 2MB pages as any amd64-capable processor supports this.
	 */
	uint64_t* pml4 = (uint64_t*)base;
	uint64_t* pdpe = (uint64_t*)(base + PAGE_SIZE);
	uint64_t* pde =  (uint64_t*)(base + 2 * PAGE_SIZE);
	for (unsigned int n = 0; n < 512; n++) {
		pde[n] = (uint64_t)((addr_t)(n << 21) | PE_PS | PE_RW | PE_P);
		pdpe[n] = (uint64_t)((addr_t)pde | PE_RW | PE_P);
		pml4[n] = (uint64_t)((addr_t)pdpe | PE_RW | PE_P);
	}

	/* All is in order - prepare for launch */
	extern void amd64_launch(uint64_t entry, void*, void*) __attribute__((noreturn));
	amd64_launch(entry, pml4, bootinfo);
}

/* vim:set ts=2 sw=2: */
