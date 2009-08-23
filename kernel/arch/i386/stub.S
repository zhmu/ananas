/*
 * Low-level initialization code.
 *
 */
.text

#include "param.h"

.global __entry, idt, gdt

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
