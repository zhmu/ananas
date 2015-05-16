#include "multiboot.h"
#include "amd64.h"
#include "bootinfo.h"
#include "io.h"
#include "lib.h"
#include "relocate.h"
#include "types.h"

void
startup(struct MULTIBOOT* mb)
{
	if (mb == NULL)
		panic("no multiboot info!");

	printf("Ananas/amd64 chainloader - flags 0x%x, memory: %d KB\n", mb->mb_flags, (mb->mb_mem_lower + mb->mb_mem_higher));

	if ((mb->mb_flags & MULTIBOOT_FLAG_MMAP) == 0)
		panic("no memory map supplied");

	/*
	 * Relocate the kernel image; this will also give us the memory range
	 * occupied by the kernel.
	 */
	struct RELOCATE_INFO ri_kernel;
	relocate_kernel(&ri_kernel);

	/* Create the bootinfo; this is required to boot the kernel */
	struct BOOTINFO* bi = create_bootinfo(&ri_kernel, mb);

	/* And... throw the switch - won't return */
	amd64_exec(ri_kernel.ri_entry, bi);
}
