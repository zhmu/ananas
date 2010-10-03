#include <loader/lib.h>
#include <loader/elf.h>
#include <loader/x86.h>
#include <loader/platform.h>
#include <ananas/i386/param.h>  /* for page-size */
#include <ananas/amd64/vm.h>    /* for PE_xxx */

#ifdef X86_64
/*
 * In x86_64 (amd64) mode, paging is mandatory - this means we have to
 * immediately map the kernel's virtual address space to the physical location
 * where we loaded it.
 *
 * For the sake of simplicity, we use 2MB pages as that is the larges page size
 * guaranteed to be available (newer revisions also have a 1GB page size, but
 * we can't rely on that)
 */

#define LARGE_PAGE_SIZE	(2 * 1024 * 1024)
#define ROUND_2MB(x) ((x) & 0xffffffffffe00000)

/* PML4 pointer */
uint64_t* pml4;

static int
x86_64_map(uint64_t dest, addr_t src, int num_pages)
{
	/* Provide some safety measures to ensure we don't attempt bad mappings */
	if ((dest >> 48) != (((dest >> 47) & 1 ? 0xffff : 0))) {
		printf("x86_64_map(): invalid destination address %x:%x, bit 47 does not match bits 48-63",
		 (int)(dest >> 32), (int)(dest));
		return 0;
	}

	if (src & 0x1fffff) {
		printf("x86_64_map(): invalid source address %x, must be 2MB-aligned\n",
		 src);
		return 0;
	}

	/*
	 * In amd64 mode with 2MB pages, dest is represented as:
	 *
	 * - sign extend            (bits 63-48): duplication of bit 47
	 * - page-map level4 table  (bits 47-39): index in the PML4 table
	 * - page-dir-pointer table (bits 38-30): index in the PML4's PDP table
	 * - page-dir offset table  (bits 29-21): index in the PDP's page-dir table
	 * - physical offset        (bits 20- 0): copied as-is
	 */
	while(num_pages--) {
		uint32_t pml4_index = (dest >> 39) & 0x1ff;
		uint32_t pdpe_index = (dest >> 30) & 0x1ff;
		uint32_t pde_index  = (dest >> 21) & 0x1ff;
		if (pml4[pml4_index] == 0) {
			/* No entry there */
			pml4[pml4_index] = (uint64_t)((addr_t)platform_get_memory(PAGE_SIZE)) | PE_RW | PE_P;
		}
		uint64_t* pdpe = (uint64_t*)((addr_t)(pml4[pml4_index] & 0xfffff000));

		if (pdpe[pdpe_index] == 0) {
			pdpe[pdpe_index] = (uint64_t)((addr_t)platform_get_memory(PAGE_SIZE)) | PE_RW | PE_P;
		}
		uint64_t* pde = (uint64_t*)((addr_t)pdpe[pdpe_index] & 0xfffff000);

		pde[pde_index] = (uint64_t)(src | PE_PS | PE_RW | PE_P);

		dest += LARGE_PAGE_SIZE; src += LARGE_PAGE_SIZE;
	}
	return 1;
}

void
x86_64_exec(struct LOADER_ELF_INFO* loadinfo, struct BOOTINFO* bootinfo)
{
	/*
	 * All pagetable entries must be page-aligned (the lower 12 bits must be
	 * unset. Use a kludge to ensure this, as we won't get very far otherwise.
 	 */
	addr_t cur_offset = (addr_t)platform_get_memory(0);
	(void)platform_get_memory(PAGE_SIZE - (cur_offset & (PAGE_SIZE - 1)));

	pml4 = platform_get_memory(PAGE_SIZE);

	/* Map the executable we loaded */
	int num_pages = (loadinfo->elf_end_addr - loadinfo->elf_start_addr + LARGE_PAGE_SIZE - 1) / LARGE_PAGE_SIZE;
	if (!x86_64_map(ROUND_2MB(loadinfo->elf_start_addr), ROUND_2MB(loadinfo->elf_phys_start_addr), num_pages))
		return;

	/* Map our loader code; it's in the first 1MB, so just do a simple 2MB mapping as that is certainly enough */
	if (!x86_64_map(0, 0, 1))
		return;

	/* All is in order - prepare for launch */
	x86_64_launch(loadinfo->elf_start_addr, bootinfo);
}
#endif /* X86_64 */

/* vim:set ts=2 sw=2: */
