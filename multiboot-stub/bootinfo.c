#include "bootinfo.h"
#include "types.h"
#include <ananas/x86/smap.h>
#include <ananas/bootinfo.h>
#include <loader/module.h>
#include "lib.h"
#include "io.h"
#include "multiboot.h"
#include "relocate.h"

static void
relocate_info2loader_module(const struct RELOCATE_INFO* ri, struct LOADER_MODULE* mod)
{
	mod->mod_bits = 64;
	mod->mod_entry = ri->ri_entry;
	mod->mod_start_addr = ri->ri_vaddr_start;
	mod->mod_end_addr = ri->ri_vaddr_end;
	mod->mod_phys_start_addr = ri->ri_paddr_start;
	mod->mod_phys_end_addr = ri->ri_paddr_end;
}

struct BOOTINFO*
create_bootinfo(const struct RELOCATE_INFO* ri_kernel, const struct MULTIBOOT* mb)
{
	/*
	 * Position the bootinfo buffer right at the end of the lower memory; this
	 * should be unused by anything. We hope 4KB is enough for now.
	 */
	uint32_t avail = (mb->mb_mem_lower - 4) * 1024;

#define ALLOC(len) \
	({ void* ptr = (void*)avail; avail += (len); ptr; })

	/* Construct the boot settings */
	struct BOOTINFO* bootinfo = ALLOC(sizeof *bootinfo);
	memset(bootinfo, 0, sizeof(*bootinfo));
	bootinfo->bi_size = sizeof(*bootinfo);

	/* Create the kernel module; this must be the first one (and currently, the only one) */
	struct LOADER_MODULE* mod_kernel = ALLOC(sizeof *mod_kernel);
	memset(mod_kernel, 0, sizeof(*mod_kernel));
	mod_kernel->mod_type = MOD_KERNEL;
	relocate_info2loader_module(ri_kernel, mod_kernel);
	bootinfo->bi_modules = (uint32_t)mod_kernel;
	bootinfo->bi_modules_size = sizeof(struct LOADER_MODULE);
	
	/*
	 * Create a memory map by traversing the multiboot-provided memory map.
	 * Note that the format is quite awkward: each entry starts with a 32-bit
	 * value containing its size, but this length is _not_ counted within the
	 * size itself....
	 */
	bootinfo->bi_memory_map_addr = avail;

	struct MULTIBOOT_MMAP* mm = (void*)mb->mb_mmap_addr;
	for (int mm_left = mb->mb_mmap_length, n = 0; mm_left > 0; n++) {
#if DEBUG
		printf("memory chunk %d: type %d phys %x:%x len %x:%x\n",
		 n, mm->mm_type, mm->mm_base_hi, mm->mm_base_lo, mm->mm_len_hi, mm->mm_len_lo);
#endif
		if (mm->mm_type == MULTIBOOT_MMAP_AVAIL) {
			struct SMAP_ENTRY* sme = ALLOC(sizeof *sme);
			sme->base_lo = mm->mm_base_lo;
			sme->base_hi = mm->mm_base_hi;
			sme->len_lo = mm->mm_len_lo;
			sme->len_hi = mm->mm_len_hi;
			sme->type = SMAP_TYPE_MEMORY;
		}

		/* Traverse to the next entry, with the length prefix-weirdness and all */
		int len = mm->mm_entry_len + sizeof(uint32_t) /* length */;
		mm_left -= len;
		mm = (struct MULTIBOOT_MMAP*)((char*)mm + len);
	}
	bootinfo->bi_memory_map_size = avail - bootinfo->bi_memory_map_addr;

	if (mb->mb_flags & MULTIBOOT_FLAG_CMDLINE) {
		/* Copy commandline arguments over */
		const char* cmdline = (const char*)mb->mb_cmdline;
		int cmdline_len = 0;
		while(cmdline[cmdline_len] != '\0')
			cmdline_len++;
		cmdline_len++; /* terminating \0 */
		char* args = ALLOC(cmdline_len);
		memcpy(args, cmdline, cmdline_len);

		bootinfo->bi_args_size = cmdline_len;
		bootinfo->bi_args = (bi_addr_t)args;
	}

#undef ALLOC
	return bootinfo;
}
