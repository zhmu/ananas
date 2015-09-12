#ifndef __ANANAS_BOOTINFO_H__
#define __ANANAS_BOOTINFO_H__

#ifndef LOADER
#include <ananas/types.h>
#endif

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
	/* Pointer and length of module chain; first module is always the kernel */
	bi_addr_t	bi_modules;
	bi_size_t	bi_modules_size;
	/* MD memory map and size */
	bi_addr_t	bi_memory_map_addr;
	bi_size_t	bi_memory_map_size;
	/* Video mode */
	uint32_t	bi_video_xres;
	uint32_t	bi_video_yres;
	uint32_t	bi_video_bpp;
	bi_addr_t	bi_video_framebuffer;
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
