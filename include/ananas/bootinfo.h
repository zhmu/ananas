#ifndef __ANANAS_BOOTINFO_H__
#define __ANANAS_BOOTINFO_H__

#include <ananas/types.h>

#ifndef ASM
struct BOOTINFO {
	uint32_t	bi_size;
	addr_t		bi_kernel_firstaddr;
	size_t		bi_kernel_size;
	addr_t		bi_ramdisk_firstaddr;
	size_t		bi_ramdisk_size;
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
