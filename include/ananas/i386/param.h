/* i386 is little endian */
#define LITTLE_ENDIAN

/* i386 is 32 bit */
#define ARCH_BITS		32

/* Page size */
#define PAGE_SIZE		4096

/* This is the base address where the kernel should be linked to */
#define KERNBASE		0xc0000000

/* Virtual address of the per-thread userland stack */
#define USERLAND_STACK_ADDR	0xa000

/* Number of Global Descriptor Table entries */
#define GDT_NUM_ENTRIES		7

/* Number of Interrupt Descriptor Table entries */
#define IDT_NUM_ENTRIES		256

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000

/* Thread stack size */
#define THREAD_STACK_SIZE	0x4000
