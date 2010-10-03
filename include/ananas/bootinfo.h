#ifndef __ANANAS_BOOTINFO_H__
#define __ANANAS_BOOTINFO_H__

#include <ananas/types.h>

#ifndef ASM
/*
 * This structure will be used by the loader, which is always 32 bit. Thus, it
 * will always store the structures in the first 4GB of memory (using addr_t
 * and such will cause problems if the kernel is 64 bit since they will not
 * match up)
 */
typedef uint32_t bi_addr_t;
typedef uint32_t bi_size_t;

struct BOOTINFO {
	uint32_t	bi_size;
	/* Kernel address and length */
	bi_addr_t	bi_kernel_addr;
	bi_size_t	bi_kernel_size;
	/* Ramdisk address and length, if any */
	bi_addr_t	bi_ramdisk_addr;
	bi_size_t	bi_ramdisk_size;
	/* MD memory map and size */
	bi_addr_t	bi_memory_map_addr;
	bi_size_t	bi_memory_map_size;
};
#endif

/*
 * Magic values so the kernel knows it's being launched with a sensible boot
 * info parameter.
 */
#define BOOTINFO_MAGIC_1	0x52505753
#define BOOTINFO_MAGIC_2	0x62303074

#if defined(KERNEL) && !defined(ASM)
/* To be supplied by the MD-code */
extern struct BOOTINFO* bootinfo;
#endif

#endif /* __ANANAS_BOOTINFO_H__ */
