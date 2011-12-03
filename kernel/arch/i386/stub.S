/*
 * Low-level initialization code.
 *
 */
.text

#include <machine/param.h>
#include <ananas/bootinfo.h>
#include "options.h"

.global __entry, idt, gdt, bootstrap_stack

/* Macro used to reallocate a virtual address to physical */
#define R(x) ((x) - KERNBASE)

__entry:
	/* Set all extra descriptors to whatever DS is pointing to */
	mov	%ds, %ax
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

	/*
	 * If we are being launched by the Ananas loader, it will place
	 * three stack arguments: BOOTINFO_MAGIC_1, bootinfo BOOTINFO_MAGIC_2.
	 * Check for this sequence and hand bootinfo to the md_startup function,
	 * or 0 if it's not there.
	 */
	xor	%ebx, %ebx
	mov	 4(%esp), %eax
	mov	12(%esp), %ecx
	cmp	$BOOTINFO_MAGIC_1, %eax
	jne	no_bootinfo
	cmp	$BOOTINFO_MAGIC_2, %ecx
	jne	no_bootinfo

	/* Got bootinfo */
	mov	 8(%esp), %ebx

no_bootinfo:

	/* don't trust what the bootloader gives us, set up a stack */
	mov	$R(bootstrap_stack), %esp

#ifdef OPTION_DEBUG_CONSOLE
	/* make sure we have a debugger console available immediately */
	call	debugcon_init
#endif

	pushl	%ebx
	call	md_startup
	/* NOTREACHED */

.align 32

	.space	0x2000
bootstrap_stack:

/*
 * Reserve space for the Global Descriptor Table / Interrupt Descriptor tables.
 * This is done here because their size is always fixed.
 */
gdt:
	.space	GDT_NUM_ENTRIES * 8

idt:
	.space	IDT_NUM_ENTRIES * 8
