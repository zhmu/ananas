.text

#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/param.h>
#include <ananas/x86/apic.h>

.globl __ap_entry, __ap_entry_end

/* Control Register flags */
#define CR0_PGE	(1 << 31)
#define	CR4_PAE	(1 << 5)
#define	CR4_PGE	(1 << 7)

/* Extended Feature Register and flags */
#define	MSR_EFER	 0xc0000080
#define	EFER_SCE	(1 << 0)
#define	EFER_LME	(1 << 8)

.code16
.align 16

/*
 * We are launched in 16 bit real mode, and must make the voyage to 64 bit long
 * mode... note that this code is relocatable, so we'll have to figure out
 * where we are first...
 */
__ap_entry:
	/*
	 * Create a stack; we use first page in memory for this since we never map it to
	 * something useful. Note that it does not matter if multiple AP's use
	 * the same stack because they will all use the same values.
	 */
	xor	%eax, %eax
	mov	%ax, %ss
	mov	$0xffc, %esp

	/* Load the AP bootstrap GDT; we need it to get to long mode */
	pushl	$(ap_bootstrap_gdt - KERNBASE)
	pushw	$(ap_bootstrap_gdt_end - ap_bootstrap_gdt)
	lgdt	(%esp)

	/* Now enable protected mode... */
	movl	$1, %ebx
	movl	%ebx, %cr0

	/* ...and jump to our 32 bit code */
	.byte	0x66
	.byte	0xea
	.long	mp_stub32 - KERNBASE
	.word	0x8

.code32

mp_stub32:
	movw	$0x10, %ax
	movw	%ax, %ds

	/*
	 * Okay, in 32-bit protected mode now; we can use the LAPIC ID to figure out
	 * which BSP we are and thus which GDT, stack etc we have to activate - we
	 * do this here.
	 */
	movl	(LAPIC_BASE + LAPIC_ID), %esi
	shrl	$24, %esi

	/* Enable PAE to prepare for long mode */
	movl	%cr4, %eax
	orl	$CR4_PAE, %eax
	movl	%eax, %cr4

	/*
	 * Note that we cannot load the kernel's pagedir here directly; this is because it will not
	 * contain an identity-mapping to here, and it's actually a horror to add it.
	 */
	movl	(smp_ap_pagedir - KERNBASE), %eax
	movl	%eax, %cr3

	/* Enable long mode */
	movl	$MSR_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Enable paging (%ebx still holds our initial %cr0) */
	orl	$CR0_PGE, %ebx
	movl	%ebx, %cr0

	/*
	 * Now we can transcend in 64 bit mode; note that we can only
	 * jump to 32-bit addresses here, so we have to adjust.
	 */
	ljmp	$0x18, $(mp_stub64 - KERNBASE)

.code64

mp_stub64:
	/*
	 * Okay, we are in 64-bit mode now, with our bootstrap pagetables which
	 * only map 1GB - ensure we're running from the correct virtual address
	 * first.
	 */
	movq	$1f, %rax
	jmp	%rax

1:
	/* Enable global pages */
	movq	%cr4, %rax
	orq	$CR4_PAE, %rax
	movq	%rax, %cr4

	/*
	 * Activate the real kernel pagetables - we need to throw the top 32
	 * bits away as they are relocated as well.
	 */
	movq	kernel_pagedir, %rax
	movl	%eax, %eax
	movq	%rax, %cr3

	hlt

.align 32

__ap_entry_end:
	nop

ap_bootstrap_gdt:
	.long	0, 0 /* 0: null */
	.long	0x0000ffff, 0x00cf9800 /* 8: 32-bit code */
	.long	0x0000ffff, 0x00cf9200 /* 10: 32-bit data */
	.long	0, 0x00209800 /* 18: 64-bit code */
	.long	0, 0x00009200 /* 20: 64-bit data */

ap_bootstrap_gdt_end:
