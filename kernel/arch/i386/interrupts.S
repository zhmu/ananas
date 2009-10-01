/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exceptionA, exceptionB
.globl exceptionC, exceptionD, exceptionE
.globl irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9
.globl irqA, irqB, irqC, irqD, irqE, irqF
.globl scheduler_irq, syscall_int

#include "i386/vm.h"
#include "param.h"

/* Basic exception handler framework */
#define EXCEPTION_HANDLER_BODY(num) \
	/* Save the previous state first */ \
	pushal; \
	push	%ds; \
	push	%es; \
	push	%fs; \
	push	%gs; \
	push	%ss; \
	/* Set up kernel data */ \
	movl	$GDT_SEL_KERNEL_DATA, %eax; \
	movw	%ax, %ds; \
	movw	%ax, %es; \
	/* Set paging table back XXX */ \
	movl	pagedir, %eax; \
	subl	$KERNBASE, %eax; \
	movl	%eax, %cr3; \
	/* Call the exception handler */ \
	pushl	$0x ## num; \
	call	exception_handler; \
	addl	$8, %esp; /* Don't restore ss register */ \
	/* Restore previous state */ \
	pop	%gs; \
	pop	%fs; \
	pop	%es; \
	pop	%ds; \
	popal

/* Basic IRQ handler framework */
#define IRQ_HANDLER_BODY(num) \
	/* Save the previous state first */ \
	pushal; \
	push	%ds; \
	push	%es; \
	push	%fs; \
	push	%gs; \
	push	%ss; \
	/* Set up kernel data */ \
	movl	$GDT_SEL_KERNEL_DATA, %eax; \
	movw	%ax, %ds; \
	movw	%ax, %es; \
	/* Acknowledge the interrupt */ \
	movb	$0x20, %al; \
	outb	%al, $0x20; \
	movl	$0x ## num, %ebx; \
	cmp	$0x8, %ebx; \
	jl	skip_pic2_ ## num; \
	outb	%al, $0xa0; \
skip_pic2_ ## num: ; \
	/* Store old cr3 */ \
	movl	%cr3, %eax; \
	pushl	%eax; \
	/* Set paging table to kernel pagetables */ \
	movl	pagedir, %eax; \
	subl	$KERNBASE, %eax; \
	movl	%eax, %cr3; \
	/* Call the exception handler */ \
	pushl	$0x ## num; \
	call	irq_handler; \
	addl	$4, %esp; \
	pop	%eax; \
	movl	%eax, %cr3; \
	addl	$4, %esp; /* Don't restore ss register */ \
	/* Restore previous state */ \
	pop	%gs; \
	pop	%fs; \
	pop	%es; \
	pop	%ds; \
	popal; \
	iret

/*
 * There exist two versions of exceptions: those with an error code, and those
 * without one. Our handler code expects to be called with an error code, so
 * we add a fake zero one if needed for those exceptions that don't have one.
 */
#define EXCEPTION_HANDLER_ERRORCODE(num) \
exception ## num: \
	pushl $0; \
	EXCEPTION_HANDLER_BODY(num); \
	addl $4, %esp;  \
	iret

#define EXCEPTION_HANDLER(num) \
exception ## num: \
	EXCEPTION_HANDLER_BODY(num); \
	iret

/* IRQ handlers are a lot simpler: they come in a single easy flavour */
#define IRQ_HANDLER(num) \
irq ## num: \
	IRQ_HANDLER_BODY(num); \
	iret

/*
 * The scheduler interrupt is pure magic; it forcefully stores the context of the
 * current thread, and then calls the scheduler, which restores the context of
 * whatever thread it wants.
 */
scheduler_irq:
	/*
	 * If we don't have a current context pointer, don't bother storing it.
	 * Note that this nukes eflags, but that doesn't matter because
	 * md_store_ctx uses the eflags stored by the interrupt
	*/
	pushl	%fs
	pushl	%eax
	movw	$0x28, %ax /* XXX */
	movw	%ax, %fs
	cmpl	$0, %fs:0
	popl	%eax
	pop	%fs
	jz	skip_store

	/* Store current context, please */
	call	md_store_ctx

skip_store:
	/* Acknowledge IRQ! */
	movb	$0x20, %al
	outb	%al, $0x20

	/* Next context */
	jmp	schedule

	/* UNREACHED - schedule never returns */

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

	popl	%edi
	popl	%esi
	popl	%ebx

	leave
	iret


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
EXCEPTION_HANDLER_ERRORCODE(A)
EXCEPTION_HANDLER_ERRORCODE(B)
EXCEPTION_HANDLER_ERRORCODE(C)
EXCEPTION_HANDLER_ERRORCODE(D)
EXCEPTION_HANDLER_ERRORCODE(E)

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
IRQ_HANDLER(A)
IRQ_HANDLER(B)
IRQ_HANDLER(C)
IRQ_HANDLER(D)
IRQ_HANDLER(E)
IRQ_HANDLER(F)
