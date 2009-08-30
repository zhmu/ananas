/*
 * Low-level initialization code.
 *
 */
.text

#include "param.h"
#include "types.h"

.global __entry, idt, gdt, temp_pt_entry

/* Macro used to reallocate a virtual address to physical */
#define R(x) ((x) - KERNBASE)

__entry:
	/* Set all extra descriptors to whatever DS is pointing to */
	mov	%ds, %ax
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

	/* don't trust what the bootloader gives us, set up a stack */
	mov	$R(KERNEL_STACK), %esp

	jmp	md_startup

.align 32

	.space	0x2000
KERNEL_STACK:

/*
 * Reserve space for the Global Descriptor Table / Interrupt Descriptor tables.
 * This is done here because their size is always fixed.
 */
gdt:
	.space	GDT_NUM_ENTRIES * 8

idt:
	.space	IDT_NUM_ENTRIES * 8

/*
 * Reserve space for a temporary page directory entry; this is needed because
 * it is the only way we can pre-setup a mapping.
 */
.align 4096
temp_pt_entry:
	.space	PAGE_SIZE
