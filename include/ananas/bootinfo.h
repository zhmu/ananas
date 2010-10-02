#ifndef __ANANAS_BOOTINFO_H__
#define __ANANAS_BOOTINFO_H__

#include <ananas/types.h>

#ifndef ASM
struct BOOTINFO {
	uint32_t	bi_size;
	/* Kernel address and length */
	addr_t		bi_kernel_addr;
	size_t		bi_kernel_size;
	/* Ramdisk address and length, if any */
	addr_t		bi_ramdisk_addr;
	size_t		bi_ramdisk_size;
	/* MD memory map and size */
	void*		bi_memory_map_addr;
	size_t		bi_memory_map_size;
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
