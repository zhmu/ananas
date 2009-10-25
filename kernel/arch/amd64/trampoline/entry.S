.text
.global __entry

#include "trampoline.h"

#define CR0_PE		1

#define E820MAP		0x08000
#define RELOC_BASE	0x10000

#define EFLAGS_CPUID	(1 << 21)
#define CPUID_LM	(1 << 29)

__entry:
	/*
	 * First of all, relocate the real-mode part to RELOC_BASE. This is
	 * necessary because we will be dropping to realmode soon, so we have
	 * to run at <=1MB.
	 */
	movl	$code16, %esi
	movl	$RELOC_BASE, %edi
	movl	$0x1000, %ecx
	rep	movsd

	/* Load the new GDT and jump to 16 bit mode */
	lgdt	(gdtr)
	ljmp	$0x28, $0

.code16

code16:
	/* We are now in 16 bit protected mode; return to real mode plz */
	movl	%cr0, %eax
	andl	$~CR0_PE, %eax
	movl	%eax, %cr0

	.byte	0xea
	.word	code16_2 - code16
	.word	RELOC_BASE >> 4

code16_2:
	/* We are now in 16 bit real mode - fix segments and a stack */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movl	$(code16_stack - code16), %esp

	/*
	 * Check for CPUID instruction first; we need it to check for long mode.
	 * The following code is based on AMD64 Architecture Programmers Manual
	 * Volume 3: General Purpose and System Functions, CPUID instruction.
	 */
	pushfl
	pop	%eax
	movl	%eax, %ebx			/* store original flags */
	xorl	$EFLAGS_CPUID, %eax		/* try to toggle CPUID bit */
	pushl	%eax
	popfl

	pushfl
	popl	%eax
	cmp	%eax, %ebx
	jnz	cpuid_ok

	/* CPU does not support cpuid */
	movw	$msg_nocpuid - code16, %si
	call	puts
	hlt

cpuid_ok:
	/*
	 * Check for Long Mode capability; the reason we do this only now is
	 * because it's far easier to show a message if it fails ;-)
 	 */
	movl	$0x80000001, %eax
	cpuid
	andl	$CPUID_LM, %edx
	orl	%edx, %edx
	jnz	lm_ok

	/* CPU does not support long mode */
	movw	$msg_nolm - code16, %si
	call	puts
	hlt

lm_ok:
	/*
	 * Clear out the registers - some bios'es do weird
	 * things if lingering high bits are set.
	 */
	xorl	%ebp, %ebp
	xorl	%esi, %esi

	/* Location of the memory map */
	movw	$E820MAP >> 4, %ax
	movw	%ax, %es
	movl	$E820MAP % 16, %edi

	xorl	%ebx, %ebx

	/*
	 * Obtain the complete memory map; this cannot be done in Long Mode,
	 * and since V8086 mode no longer exists, we have to do it in real
	 * mode - better do it before wiring the system for LM operation then.
	 */
mmap:
	movl	$0xe820, %eax			/* Obtain E820 memory map */
	movl	$20, %ecx			/* Entry size */
	movl	$0x534d4150, %edx		/* SMAP */
	int	$0x15

	cmp	$0x534d4150, %eax
	jne	mmap_done

	add	$20, %di

	orl	%ebx, %ebx
	jnz	mmap

mmap_done:
	/* End the memory map with a sequence of 0xff */
	movb	$0xff, %al
	movl	$20, %ecx
	rep	stosb

	/* We have what we need - enable 32 bit protected mode */
	movl	%cr0, %eax
	orl	$CR0_PE, %eax
	movl	%eax, %cr0

	/* Return back to 32 bit code */
	.byte	0x66
	.byte	0xea
	.long	code32
	.word	0x18

puts:
	mov	$0xe, %ah

puts_loop:
	lodsb
	orb	%al, %al
	jz	puts_done

	int	$0x10
	jmp	puts_loop

puts_done:
	ret

	.space	0x100
code16_stack:

msg_nocpuid:
	.ascii	"CPU does not support CPUID instruction"
	.byte	0

msg_nolm:
	.ascii	"CPU does not support Long Mode",
	.byte	0

.code32

code32:
	/* Restore our segment registers */
	movw	$0x20, %ax
	movw	%ax, %ds
	movw	%ax, %es

	/* Set up a stack */	
	movw	%ax, %ss
	movl	$STACK, %esp

	/* Wire things up for long mode operation */
	pushl	$E820MAP
	call	startup
	add	$4, %esp

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
	movw	%ax, %es

	/* Obtain bootinfo structure; this is passed to the kernel as %rsi */
	xorq	%rax, %rax
	movl	$bootinfo, %eax
	movl	(%rax), %edi

	/* Transfer control to the ELF file we loaded */
	movl	$kernel_rip, %eax
	call	*(%rax)

	/* Why are we here? */
	hlt

e820map:
gdtr:	.word	(gdt_end - gdt)
	.long	gdt

.align 16

gdt:	.long	0			/*  0: null descriptor */
	.long	0

	.long	0         		/*  8: 64-bit code */
	.long	0x00209800

	.long	0         		/* 10: 64-bit data */
	.long	0x00009000

	.long	0x0000ffff		/* 18: 32-bit code */
	.long	0x00cf9800

	.long	0x0000ffff		/* 20: 32-bit data */
	.long	0x00cf9200

	.long	0x0000ffff		/* 28: 16-bit code */
	.long	0x00009800 | (RELOC_BASE >> 16)

	.long	0x0000ffff		/* 30: 16-bit data */
	.long	0x00009200 | (RELOC_BASE >> 16)

gdt_end:

	.space	0x1000
STACK:
