.text
.code16

#include <machine/param.h>
#include <machine/vm.h>

.globl realmode16_stub, realmode16_code
.globl realmode_regs, realmode_int, realmode_store

#define R(x) ((x) - KERNBASE)

.align 4096

realmode16_stub:
	/* Store ebp; the compiler depends on it */
	pushl	%ebp

	/* Store esp; we need to restore it once we leave protected mode */
	movl	$R(esp_store), %esi
	mov	%esp,(%esi)

	/* Get rid of paging *sniff* */
	movl	%cr0, %eax
	andl	$0x7fffffff, %eax
	movl	%eax, %cr0

	/* Clear out cr3 */
	xor	%eax, %eax
	movl	%eax, %cr3

	/*
	 * OK, once we are here, paging is disabled. This means
	 * we can safely move the realmode stub around to
	 * a location where realmode is happy.
	 */
	movl	$R(realmode16_code), %esi
	movl	$REALSTUB_RELOC, %edi
	movl	$(realmode16_end - realmode16_code), %ecx
	rep addr32 movsb

	/* Set segment registers to the 16 bit versions */
	mov	$GDT_SEL_KERNEL_DATA16, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss

	/* Clear out FS/GS, they are not needed */
	xor	%ax, %ax
	mov	%ax, %fs
	mov	%ax, %gs

	/* Now, we can leave protected mode */
	movl	%cr0, %eax
	andl	$0xfffffffe, %eax
	movl	%eax, %cr0

	/* Execute a JMP FAR to our real-mode location */
	ljmp	$REALSTUB_RELOC / 16, $0

realmode16_code:
	/*
	 * This code will be executed in 16 bit real mode. Start by
	 * preparing our segment registers.
	 */
	mov	%cs, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss

	/* We just 2 bytes of stack space ... */
	movl	$realmode_regs, %eax
	movzx	%ax, %eax
	movl	%eax, %esp

	call	n

	/* this is the stack usuable by the program */
	.space	64

realmode_regs:
	.space	32

n:	pop	%sp
	add	$64, %esp		/* skip temp stack */
	popal

	/*
	 * We are now ready to invoke the interrupt.
	 */
	.byte	0xcd		/* INTxx */
realmode_int:	.byte 0x03

	/* We are done. Store the registers */
	pushal

	/*
	 * Make the journey back to 32 bit protected mode; this
	 * consists of creating a gdtr register and loading it.
	 *
	 * Since our GDT is always fixed, this is actually rather
	 * easy - we just figure out the location and go!
	 */
	call	m
gdtr:	.word	(GDT_NUM_ENTRIES * 8) - 1
	.long	gdt - KERNBASE
m:	pop	%si
	lgdt	(%si)

	/* Return to protected mode */
	mov	%cr0, %eax
	orl	$0x1, %eax
	mov	%eax, %cr0 

	/*
	 * We must do a JMP FAR to the 32 bit stub. Note that there
	 * is no paging yet, so we must relocate the addresses.
	 */
	.byte	0x66
	.byte	0xea
	.long	stub32 - KERNBASE
	.word	GDT_SEL_KERNEL_CODE

esp_store:
	.long	0

	/*
	 * Provide a location that can be used for real-mode data
	 * transfer. We'll enlarge it if more is needed some day... 
	 */
realmode_store:
	.space	64

.code32
stub32:
	/*
	 * We are now, once again, in 32 bit protected mode.
	 */

	/* Restore 32 bit registers */
	mov	$GDT_SEL_KERNEL_DATA, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

	/*
	 * Before we restore paging, we should copy the relocated
	 * registers back; there's no other way to read them once
	 * we've enabled paging...
	 */
	movl	$(REALSTUB_RELOC + realmode_regs - realmode16_code), %esi
	movl	$R(realmode_regs), %edi
	movl	$0x8, %ecx
	rep	movsd

	/* Do the same for the data store */
	movl	$(REALSTUB_RELOC + realmode_store - realmode16_code), %esi
	movl	$R(realmode_store), %edi
	movl	$0x10, %ecx
	rep	movsd

	/* Restore paging! */
	movl	$pagedir, %eax
	subl	$KERNBASE, %eax
	movl	(%eax), %eax
	subl	$KERNBASE, %eax
	movl	%eax, %cr3

	movl	%cr0, %eax
	orl	$0x80000000, %eax
	movl	%eax, %cr0

	/* Restore ss:esp */
	mov	$GDT_SEL_KERNEL_DATA, %ax
	mov	%ax, %ss
	movl	$R(esp_store), %esi
	mov	(%esi),%esp

	popl	%ebp

	lret

realmode16_end:	
