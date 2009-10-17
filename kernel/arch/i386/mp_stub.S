.text

#include "i386/vm.h"
#include "i386/macro.h"
#include "param.h"

.globl __ap_entry, __ap_entry_end, mp_ap_startup, panic
.globl	lapic2cpuid, cpus

.code16
.align 16

__ap_entry:
	nop
	nop

	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es

	/* Load the GDT */
	call	m
gdtr:	.word	(GDT_NUM_ENTRIES * 8) - 1
	.long	gdt - KERNBASE
m:	pop	%si
	lgdt	(%si)

	/*
	 * Calculate the offset of mp_stub32; we do this because we are
	 * sure a mapping to there exists. Without this trickery, we would
	 * have to identify-map mp_stub32 - KERNBASE to mp_stub32, which is
	 * much more work...
	 */
	movzx	%ax, %ebx
	shl	$0x4, %ebx
	add	$(mp_stub32 -  __ap_entry), %ebx

	/* Jump to protected mode */
	mov	%cr0, %eax
	orl	$0x1, %eax
	mov	%eax, %cr0 

	/*
	 * We must do a JMP FAR to the 32 bit stub. Note that there
	 * is no paging yet, so we must relocate the addresses.
	 */
	.byte	0x66
	.byte	0xea
	.long	mp_stub32_jmp - KERNBASE
	.word	GDT_SEL_KERNEL_CODE

.code32

mp_stub32_jmp:
	jmp	*%ebx

mp_stub32:
	/* Restore 32 bit registers */
	mov	$GDT_SEL_KERNEL_DATA, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/*
	 * Locate our APIC ID; we use this to determine which stack we have to
	 * use.
	 */
	movl	0xfee00020, %ebx
	shrl	$24, %ebx

	/* Set up the page directory */
	movl	$pagedir, %eax
	subl	$KERNBASE, %eax
	movl	(%eax), %eax
	subl	$KERNBASE, %eax
	movl	%eax, %cr3

	/* Enable paging */
	movl	%cr0, %eax
	orl	$0x80000000, %eax
	movl	%eax, %cr0

	/*
	 * OK, we have paging now; this means we can use our real
	 * address space, which will make things like 'call' work.
	 */
	movl	$mp_stub32_paging, %eax
	jmp	*%eax

mp_stub32_paging:
	/* Time to switch to a stack */
	movl	lapic2cpuid, %edx
	addl	%ebx, %edx
	xor	%eax, %eax
	mov	(%edx), %al		/* eax = lapic2cpuid[lapicid] */
	shl	$2, %eax
	addl	cpus, %eax
	movl	(%eax), %eax		/* eax = cpus[lapic2cpuid[lapicid]] */
	movl	4(%eax), %esp
	movl	8(%eax), %edx		/* edx = private gdt */

	call	pm
	.word	(GDT_NUM_ENTRIES * 8) - 1
	.long	0
pm:	pop	%esi
	movl	%edx, 2(%esi)		/* overwrite gdt address above */
	lgdt	(%esi)

	call	pn
	.word	(IDT_NUM_ENTRIES * 8) - 1
	.long	idt
pn: 	pop	%esi
	lidt	(%esi)
	
	/* Call mp_ap_startup with our corresponding lapic id */
	pushl	%ebx
	call	mp_ap_startup
	addl	$4, %esp

	/* Why are we here? */
	movl	0xfee00020, %eax
	shr	$24, %eax
	pushl	%eax
	pushl	$mp_panicmsg
	call	panic
	/* NOTREACHED */

mp_panicmsg:
	.ascii	"mp_ap_startup() returned; lapic_id=%u"

.align 4
__ap_entry_end:
	nop
