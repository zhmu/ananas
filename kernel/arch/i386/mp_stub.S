.text

#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/param.h>
#include "asmsyms.h"

.globl __ap_entry, __ap_entry_end, mp_ap_startup

.code16
.align 16

__ap_entry:
	nop
	nop

	/*
	 * Create a stack; we use first page in memory for this since we never map it to
	 * something useful. Note that it does not matter if multiple AP's use
	 * the same stack because they will all use the same value.
	 */
	xor	%ax, %ax
	mov	%ax, %ss
	mov	$0xffc, %esp

	/* Load the BSP GDT, this allows us to jump to protected mode */
	pushl	$(gdt - KERNBASE)
	pushw	$((GDT_NUM_ENTRIES * 8) - 1)
	lgdt	(%esp)

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
	.long	mp_stub32 - KERNBASE
	.word	GDT_SEL_KERNEL_CODE

.code32

mp_stub32:
	/* Restore 32 bit registers */
	mov	$GDT_SEL_KERNEL_DATA, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %gs
	mov	%ax, %ss
	mov	$GDT_SEL_KERNEL_PCPU, %ax
	mov	%ax, %fs

	/*
	 * Locate our APIC ID; we use this to determine which stack we have to
	 * use.
	 */
	movl	0xfee00020, %ebx
	shrl	$24, %ebx

	/* Set up the page directory */
	movl	$kernel_pd, %eax
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
	movl	$1f, %eax
	jmp	*%eax

1:
	/*
	 * Scan for our current IA32_CPU structure; this contains the stack we
	 * have to use. Our local APIC ID is still in %ebx, we'll use that as
	 * key within the table
	 */
	movl	$smp_config, %esi
	mov	SMP_NUM_CPUS(%esi), %ecx
	movl	SMP_CPU_OFFSET(%esi), %esi

1:	cmpl	SMP_CPU_LAPICID(%esi), %ebx
	jz	2f
	add	$SMP_CPU_SIZE, %esi
	loop	1b

	/* CPU not found; now what XXX */
1:	hlt
	jmp	1b

2:	/* We found the current AP - activate our stack */
	movl	SMP_CPU_STACK(%esi), %esp

	/* Use the CPU-specific GDT */
	pushl	SMP_CPU_GDT(%esi)
	pushw	$((GDT_NUM_ENTRIES * 8) - 1)
	lgdt	(%esp)

	/* BSP's IDT is fine, so use that */
	pushl	$idt
	pushw	$((IDT_NUM_ENTRIES * 8) - 1)
	lidt	(%esp)

	/* Throw away the [gi]dtr from the stack, they are no longer needed */
	addl	$12, %esp

	/* Force a reload of the per-CPU descriptor; it has changed */
	mov	$GDT_SEL_KERNEL_PCPU, %ax
	mov	%ax, %fs

	/* Load the task register */
	movw	$GDT_SEL_KERNEL_TASK, %ax
	ltr	%ax
	
	/* Call mp_ap_startup with our corresponding lapic id */
	pushl	%ebx
	call	mp_ap_startup
	addl	$4, %esp

	/* Why are we here? */
	movl	0xfee00020, %eax
	shr	$24, %eax
	pushl	%eax
	pushl	$mp_panicmsg
	pushl	$0
	pushl	$0
	pushl	$0
	call	_panic
	/* NOTREACHED */

mp_panicmsg:
	.ascii	"mp_ap_startup() returned; lapic_id=%u"

.align 4
__ap_entry_end:
	nop
