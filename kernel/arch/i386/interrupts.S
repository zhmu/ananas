/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exceptionA, exceptionB
.globl exceptionC, exceptionD, exceptionE
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
	EXCEPTION_HANDLER_BODY(num);
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

	call	md_store_ctx

skip_store:
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
	movl	%eax, -24(%ebp)
	movl	%ebx, -20(%ebp)
	movl	%ecx, -16(%ebp)
	movl	%edx, -12(%ebp)
	movl	%esi,  -8(%ebp)
	movl	%edi,  -4(%ebp)

	/*
	 * Calculate the address to the base of the structure; this is passed
	 * to the syscall function.
	 */
	movl	%ebp, %eax
	subl	$24,  %eax
	movl	%eax, (%ebp)

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
	pushl	(%ebp)
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
