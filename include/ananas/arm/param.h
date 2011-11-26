/* ARM is bi-endian, but big endian is deprecated, so we'll use little endian */
#define LITTLE_ENDIAN

/* ARM is 32 bit */
#define ARCH_BITS		32

/* Page size */
#define PAGE_SIZE		4096

/* This is the base address where the kernel should be linked to */
#define KERNBASE		0xc0000000

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000

/* Thread stack size */
#define THREAD_STACK_SIZE	0x2000
