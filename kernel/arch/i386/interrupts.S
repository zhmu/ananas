/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19
.globl irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9
.globl irq10, irq11, irq12, irq13, irq14, irq15
.globl scheduler_irq, syscall_int, spurious_irq

#include "options.h"
#include "machine/param.h"
#include "machine/vm.h"
#include "asmsyms.h"

#define SAVE_REGISTERS \
	movl	%eax, SF_EAX(%esp); \
	movl	%ebx, SF_EBX(%esp); \
	movl	%ecx, SF_ECX(%esp); \
	movl	%edx, SF_EDX(%esp); \
	movl	%ebp, SF_EBP(%esp); \
	movl	%esp, SF_ESP(%esp); \
	movl	%edi, SF_EDI(%esp); \
	movl	%esi, SF_ESI(%esp);

#define SAVE_SEGS \
	xorl	%eax, %eax; \
	movw	%ds, %ax; \
	movl	%eax, SF_DS(%esp); \
	movw	%es, %ax; \
	movl	%eax, SF_ES(%esp); \
	movw	%fs, %ax; \
	movl	%eax, SF_FS(%esp); \
	movw	%gs, %ax; \
	movl	%eax, SF_GS(%esp);

#define RESTORE_REGISTERS \
	movl	SF_EAX(%esp), %eax; \
	movl	SF_EBX(%esp), %ebx; \
	movl	SF_ECX(%esp), %ecx; \
	movl	SF_EDX(%esp), %edx; \
	movl	SF_EBP(%esp), %ebp; \
	/* movl	SF_ESP(%esp), %esp; */ \
	movl	SF_EDI(%esp), %edi; \
	movl	SF_ESI(%esp), %esi;

#define RESTORE_SEGS \
	movl	SF_DS(%esp), %eax; \
	movw	%ax, %ds; \
	movl	SF_ES(%esp), %eax; \
	movw	%ax, %es; \
	movl	SF_FS(%esp), %eax; \
	movw	%ax, %fs; \
	movl	SF_GS(%esp), %eax; \
	movw	%ax, %gs;

eoi:
	/* interrupt to EOI is in %edx */
#ifdef SMP
	movl	$0xfee000b0, %eax
	movl	$0x0, (%eax)
#else
	movb	$0x20, %al
	outb	%al, $0x20
	cmp	$0x8, %dl
	jl	skip_pic2
	outb	%al, $0xa0
skip_pic2:
#endif
	ret

do_irq:
	SAVE_REGISTERS
	SAVE_SEGS

	/* Set up kernel data */
	movl	$GDT_SEL_KERNEL_DATA, %eax
	movw	%ax, %ds
	movw	%ax, %es

	/* Store trapframe number %edx; this is our IRQ number */
	movl	SF_TRAPNO(%esp), %edx

	/* Set paging table back */
	movl	%cr3, %ebx
	pushl	%ebx
	movl	pagedir, %eax
	subl	$KERNBASE, %eax
	movl	%eax, %cr3

	/* Ensure per-cpu stuff works */
	movl	$GDT_SEL_KERNEL_PCPU, %eax
	movw	%ax, %fs

	/* Call the IRQ handler, using stored irq number in %edx */
	pushl	%edx
	call	irq_handler
	popl	%edx

	/* Acknowledge the interrupt */
	call	eoi

	popl	%ebx
	movl	%ebx, %cr3

	/* Restore previous state */
	RESTORE_SEGS
	RESTORE_REGISTERS
	addl	$SF_EIP, %esp
	iretl

do_exception:
	SAVE_REGISTERS
	SAVE_SEGS

	/* Set up kernel data */
	movl	$GDT_SEL_KERNEL_DATA, %eax
	movw	%ax, %ds
	movw	%ax, %es

	/* Set paging table back */
	movl	%cr3, %ebx
	pushl	%ebx
	movl	pagedir, %eax
	subl	$KERNBASE, %eax
	movl	%eax, %cr3

	/* Ensure per-cpu stuff works */
	movl	$GDT_SEL_KERNEL_PCPU, %eax
	movw	%ax, %fs

	/*
	 * Call the exception handler. Note that we have hand %esp + 4 as the
	 * argument, because we have placed our saved %cr3 on the stack.
	 */
	movl	%esp, %eax
	addl	$4, %eax
	pushl	%eax
	call	exception_handler
	addl	$4, %esp

	popl	%ebx
	movl	%ebx, %cr3

	/* Restore previous state */
	RESTORE_SEGS
	RESTORE_REGISTERS
	addl	$SF_EIP, %esp
	iretl

/*
 * There exist two versions of exceptions: those with an error code, and those
 * without one. Our handler code expects to be called with an error code, so
 * we add a fake zero one if needed for those exceptions that don't have one.
 */
#define EXCEPTION_HANDLER_ERRORCODE(num) \
exception ## num: \
	subl	$SF_ERRNUM, %esp; \
	movl	$num, SF_TRAPNO(%esp); \
	jmp	do_exception

#define EXCEPTION_HANDLER(num) \
exception ## num: \
	subl	$SF_EIP, %esp; \
	movl	$num, SF_TRAPNO(%esp); \
	movl	$0, SF_ERRNUM(%esp); \
	jmp	do_exception

/* IRQ handlers are a lot simpler: they come in a single easy flavour */
#define IRQ_HANDLER(num) \
irq ## num: \
	subl	$SF_EIP, %esp; \
	movl	$num, SF_TRAPNO(%esp); \
	jmp	do_irq

/*
 * The scheduler interrupt is pure magic; it forcefully stores the context of the
 * current thread, and then calls the scheduler, which restores the context of
 * whatever thread it wants.
 *
 * Note that the scheduler is effectively disabled using the 'scheduler_active'
 * global variable - we need this during early startup when we are probing
 * devices (as interrupts will be enabled)
 */
scheduler_irq:
	/* If the scheduler is not yet enabled, go away */
	cmpl	$0, %cs:scheduler_active
	je	scheduler_cancel

	/*
	 * If we don't have a current context pointer, don't bother storing it.
	 * Note that this nukes eflags, but that doesn't matter because
	 * md_store_ctx uses the eflags stored by the interrupt
	*/
	pushl	%fs
	pushl	%eax
	movw	$GDT_SEL_KERNEL_PCPU, %ax
	movw	%ax, %fs
	cmpl	$0, %fs:0
	popl	%eax
	pop	%fs
	jz	skip_store

	/* Store current context, please */
	call	md_store_ctx

skip_store:
	/* Acknowledge IRQ! */
	xorl	%edx, %edx
	call	eoi

	/* Restore per-cpu context in fs */
	movw	$GDT_SEL_KERNEL_PCPU, %ax
	movw	%ax, %fs

	/* Next context */
	jmp	schedule
	/* UNREACHED - schedule never returns */

scheduler_cancel:
	/* Acknowledge the interrupt before exiting */
	pushl	%eax
	pushl	%edx
	xorl	%edx, %edx
	call	eoi
	popl	%edx
	popl	%eax
	iret

/*
 * System call interrupt; we expect to be called with %eax the syscall function,
 * and %ebx, %ecx, %edx, %esi and %edi the arguments. We only preserve what's
 * needed by the System V ABI because that's what everyone seems to use anyway.
 */
syscall_int:
	pushl	%ebp
	movl	%esp, %ebp
	subl	$32, %esp

	/*
	 * System V ABI for Intel386 Architecture says we only have to
	 * preserve %ebx, %esi and %edi.
	 */
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	/* Save segment registers as well; we only use ds/es/fs */
	pushl	%ds
	pushl	%es
	pushl	%fs

	/*
	 * Set up a pointer to the structure, and copy 6 arguments
	 * in place. Note that the stack grows backward, so we need
	 * to place them in reverse order.
	 */
	movl	%eax, -28(%ebp)
	movl	%ebx, -24(%ebp)
	movl	%ecx, -20(%ebp)
	movl	%edx, -16(%ebp)
	movl	%esi, -12(%ebp)
	movl	%edi,  -8(%ebp)

	/*
	 * Calculate the address to the base of the structure; this is passed
	 * to the syscall function.
	 */
	movl	%ebp, %eax
	subl	$28,  %eax
	movl	%eax, -4(%ebp)

	/* Switch to the kernel pagetables */
	movl	%cr3, %eax
	pushl	%eax
	movl	pagedir, %eax
	subl	$KERNBASE, %eax
	movl	%eax, %cr3

	/* Switch to kernel register context */
	movw	$GDT_SEL_KERNEL_DATA, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	$GDT_SEL_KERNEL_PCPU, %ax
	movw	%ax, %fs

	/*
	 * Invoke the generic syscall hander; return values will be passed in
	 * %eax, so we cannot thrash that.
	 */
	pushl	-4(%ebp)
	call	syscall
	addl	$4, %esp

	/* Restore paging and the other registers */
	popl	%ebx
	movl	%ebx, %cr3

	popl	%fs
	popl	%es
	popl	%ds

	popl	%edi
	popl	%esi
	popl	%ebx

	leave
	iret

#ifdef SMP
spurious_irq:
	iret
#endif


/* Now we just need to list the exception handlers */
EXCEPTION_HANDLER(0)
EXCEPTION_HANDLER(1)
EXCEPTION_HANDLER(2)
EXCEPTION_HANDLER(3)
EXCEPTION_HANDLER(4)
EXCEPTION_HANDLER(5)
EXCEPTION_HANDLER(6)
EXCEPTION_HANDLER(7)
EXCEPTION_HANDLER_ERRORCODE(8)
EXCEPTION_HANDLER(9)
EXCEPTION_HANDLER_ERRORCODE(10)
EXCEPTION_HANDLER_ERRORCODE(11)
EXCEPTION_HANDLER_ERRORCODE(12)
EXCEPTION_HANDLER_ERRORCODE(13)
EXCEPTION_HANDLER_ERRORCODE(14)
EXCEPTION_HANDLER(16)
EXCEPTION_HANDLER_ERRORCODE(17)
EXCEPTION_HANDLER(18)
EXCEPTION_HANDLER(19)

/* ...and the IRQ handlers */
IRQ_HANDLER(1)
IRQ_HANDLER(2)
IRQ_HANDLER(3)
IRQ_HANDLER(4)
IRQ_HANDLER(5)
IRQ_HANDLER(6)
IRQ_HANDLER(7)
IRQ_HANDLER(8)
IRQ_HANDLER(9)
IRQ_HANDLER(10)
IRQ_HANDLER(11)
IRQ_HANDLER(12)
IRQ_HANDLER(13)
IRQ_HANDLER(14)
IRQ_HANDLER(15)
