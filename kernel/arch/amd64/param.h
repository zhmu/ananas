/* This is the base address where the kernel should be linked to */
#define KERNBASE	0xffffffff80000000

/* Number of Global Descriptor Table entries */
#define GDT_NUM_ENTRIES	9

/* Number of Interrupt Descriptor Table entries */
#define IDT_NUM_ENTRIES	256
