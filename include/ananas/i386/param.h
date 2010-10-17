/* i386 is little endian */
#define LITTLE_ENDIAN

/* Page size */
#define PAGE_SIZE		4096

/* This is the base address where the kernel should be linked to */
#define KERNBASE		0xc0000000

/* Temporary mapping address for userland - will shift per CPU */
#define TEMP_USERLAND_ADDR	0xbfff0000

/* Virtual address of the per-thread stack */
#define USERLAND_STACK_ADDR	0xa000

/* Temporary mapping address size */
#define TEMP_USERLAND_SIZE	PAGE_SIZE

/*
 * This is the location where our realmode stub lives; this must
 * be <1MB because it's executed in real mode.
 */
#define REALSTUB_RELOC		0x00010000

/* Number of Global Descriptor Table entries */
#define GDT_NUM_ENTRIES		7

/* Number of Interrupt Descriptor Table entries */
#define IDT_NUM_ENTRIES		256

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000

/* Thread stack size */
#define THREAD_STACK_SIZE	0x2000
