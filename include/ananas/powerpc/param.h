/* PowerPC is bi-endian, but we use big endian like everyone else */
#define BIG_ENDIAN

/* PowerPC is 32 bit */
#define ARCH_BITS		32

/* Page size */
#define PAGE_SIZE		4096

/* This is the base address where the kernel should be linked to */
#define KERNBASE		0x0100000

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000

/* Thread stack size */
#define THREAD_STACK_SIZE	0x2000

/*
 * Temporary address used for mapping thread pointers. Must have only the top
 * 16 bits set.
 */
#define THREAD_MAP_ADDR		0xa0000000
