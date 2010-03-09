/* Page size */
#define PAGE_SIZE		4096

/* This is the base address where the kernel should be linked to */
#define KERNBASE		0xffffffff80000000

/* Temporary mapping address for userland - will shift per CPU */
#define TEMP_USERLAND_ADDR	0xfffffffffff00000

/* Temporary mapping address size */
#define TEMP_USERLAND_SIZE	PAGE_SIZE

/* Number of Global Descriptor Table entries */
#define GDT_NUM_ENTRIES		9

/* Number of Interrupt Descriptor Table entries */
#define IDT_NUM_ENTRIES		256

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000

/* Thread stack size */
#define THREAD_STACK_SIZE	0x1000
