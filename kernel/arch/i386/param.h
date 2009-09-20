/* This is the base address where the kernel should be linked to */
#define KERNBASE	0xc0000000

/*
 * This is the location where our realmode stub lives; this must
 * be <1MB because it's executed in real mode.
 */
#define REALSTUB_RELOC	0x00010000

/* Maximum number of concurrent tasks we can handle */
#define MAX_TASKS	32

/* Number of Global Descriptor Table entries */
#define GDT_NUM_ENTRIES	(8 + MAX_TASKS)

/* Number of Interrupt Descriptor Table entries */
#define IDT_NUM_ENTRIES	(256)

/* Kernel stack size */
#define KERNEL_STACK_SIZE	0x2000
