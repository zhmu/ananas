.text
.global __entry

#include "trampoline.h"

__entry:

	/* XXX switch to real mode and obtain memory map */

	movw	%ds, %ax
	movw	%ax, %es

	/* Set up a stack */	
	movw	%ax, %ss
	movl	$STACK, %esp

	/* Wire things up for long mode operation */
	call	startup

	/* Load our GDT */
	lgdt	(gdtr)

	/* Enable PAE */
	movl	%cr4, %eax
	orl	$CR4_PAE, %eax
	movl	%eax, %cr4

	/* Enable paging table */
	movl	(pml4), %eax
	movl	%eax, %cr3

	/* Enable Long Mode */
	movl	$MSR_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Enable paging */
	movl	%cr0, %eax
	orl	$CR0_PGE, %eax
	movl	%eax, %cr0

	/* Jump to our 64 bit code */
	ljmp	$8, $entry64

.code64

entry64:
	movq	$0x10, %rax
	movw	%ax, %ds

	/* Transfer control to the ELF file we loaded */
	xorq	%rax, %rax
	movl	$kernel_rip, %eax
	call	*(%rax)

	/* Why are we here? */
	hlt

gdtr:	.word	(gdt_end - gdt)
	.long	gdt

.align 16

gdt:	.long	0			/*  0: null descriptor */
	.long	0

	.long	0xffffffff		/*  8: 64-bit code, present, long */
	.long	0x00209800

	.long	0xffffffff		/* 10: 64-bir data, present, long */
	.long	0x00009000

gdt_end:

	.space	0x1000
STACK:
