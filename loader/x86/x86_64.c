#include <loader/lib.h>
#include <loader/module.h>
#include <loader/x86.h>
#include <loader/platform.h>
#include <ananas/i386/param.h>  /* for page-size */
#include <ananas/amd64/vm.h>    /* for PE_xxx */

#ifdef X86_64

/* PML4 pointer */
uint64_t* pml4;

void
x86_64_exec(struct LOADER_MODULE* mod, struct BOOTINFO* bootinfo)
{
	/*
	 * All pagetable entries must be page-aligned (the lower 12 bits must be
	 * unset. Use a kludge to ensure this, as we won't get very far otherwise.
 	 */
	addr_t cur_offset = (addr_t)platform_get_memory(0);
	(void)platform_get_memory(PAGE_SIZE - (cur_offset & (PAGE_SIZE - 1)));

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
	pml4 = platform_get_memory(PAGE_SIZE);
	uint64_t* pdpe = platform_get_memory(PAGE_SIZE);
	uint64_t* pde = platform_get_memory(PAGE_SIZE);
	for (unsigned int n = 0; n < 512; n++) {
		pde[n] = (uint64_t)((addr_t)(n << 21) | PE_PS | PE_RW | PE_P);
		pdpe[n] = (uint64_t)((addr_t)pde | PE_RW | PE_P);
		pml4[n] = (uint64_t)((addr_t)pdpe | PE_RW | PE_P);
	}

	/* All is in order - prepare for launch */
	x86_64_launch(mod->mod_start_addr, bootinfo);
}
#endif /* X86_64 */

/* vim:set ts=2 sw=2: */
