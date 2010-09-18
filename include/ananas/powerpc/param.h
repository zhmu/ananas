/* PowerPC is bi-endian, but we use big endian like everyone else */
#define BIG_ENDIAN

/* Page size */
#define PAGE_SIZE		4096

/* This is the base address where the kernel should be linked to */
#define KERNBASE		0x0100000

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000

/* Thread stack size */
#define THREAD_STACK_SIZE	0x2000
