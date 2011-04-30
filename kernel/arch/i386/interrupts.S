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
.globl clone_return

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
	cmpl	$0, lapic_initialized
	jz	eoi_pic
	movl	$0xfee000b0, %eax
	movl	$0x0, (%eax)
	ret
eoi_pic:
#endif
	movb	$0x20, %al
	outb	%al, $0x20
	cmp	$0x8, %dl
	jl	skip_pic2
	outb	%al, $0xa0
skip_pic2:
	ret

do_irq:
	SAVE_REGISTERS
	SAVE_SEGS

	/* Set up kernel data */
	movl	$GDT_SEL_KERNEL_DATA, %eax
	movw	%ax, %ds
	movw	%ax, %es

	/* Ensure per-cpu stuff works */
	movl	$GDT_SEL_KERNEL_PCPU, %eax
	movw	%ax, %fs

	/* Call the interrupt handler */
	pushl	%esp
	call	interrupt_handler
	addl	$4, %esp

	/* IRQ handler is done; it's safe to acknowledge the interrupt now */
	movl	SF_TRAPNO(%esp), %edx
	call	eoi

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

	/* Ensure per-cpu stuff works */
	movl	$GDT_SEL_KERNEL_PCPU, %eax
	movw	%ax, %fs

	/*
	 * Call the exception handler.
	 */
	pushl	%esp
	call	exception_handler
	addl	$4, %esp

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
 */
scheduler_irq:
	/*
	 * If we don't have a current context pointer, we just continue. This
	 * indicates that the scheduling isn't set up yet and thus, there's
	 * nothing for us here.
	 */
	pushl	%fs
	pushl	%eax
	movw	$GDT_SEL_KERNEL_PCPU, %ax
	movw	%ax, %fs
	incl	%fs:(PCPU_TICKCOUNT)
	cmpl	$0, %fs:(PCPU_CONTEXT)
	popl	%eax
	pop	%fs
	jz	scheduler_cancel

	/* Store current context, please */
	call	md_store_ctx

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
 * System call interrupt; system call number should be placed in %eax, all
 * arguments are expected to be placed on the stack.  Note that we only
 * preserve what's needed by the System V ABI because that's what everyone
 * seems to use anyway.
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
	 * Fetch the caller's stack pointer; this will be saved at our
	 * current stack - but note that we have already created a
	 * stackframe, so we just use that as base.
	 *
	 * The 16 comes from stored %ebp, %eip, %cs, %flags, %esp, %ss
	 * as they occur on offsets 0     4     8    12      16
	 */
	movl 16(%ebp), %esi

	/* Switch to kernel registers */ 
	movw	$GDT_SEL_KERNEL_PCPU, %bx
	movw	%bx, %fs

	/*
	 * Set up a pointer to the structure, and copy 6 arguments
	 * in place. Note that the stack grows backward, so we need
	 * to place them in reverse order.
	 */
	movl	%eax, -28(%ebp)
	movl	 4(%esi), %eax
	movl	 8(%esi), %ebx
	movl	12(%esi), %ecx
	movl	16(%esi), %edx
	movl	20(%esi), %edi
	movl	%eax, -24(%ebp)
	movl	%ebx, -20(%ebp)
	movl	%ecx, -16(%ebp)
	movl	%edx, -12(%ebp)
	movl	%edi,  -8(%ebp)

	/*
	 * Calculate the address to the base of the structure; this is passed
	 * to the syscall function.
	 */
	movl	%ebp, %eax
	subl	$28,  %eax
	movl	%eax, -4(%ebp)

	/* Switch to kernel segment register context (ds/es/fs) */
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

	/* Restore registers */
	popl	%fs
	popl	%es
	popl	%ds

	popl	%edi
	popl	%esi
	popl	%ebx

	leave
	iret

clone_return:
	/*
	 * Once here, the to-be-cloned thread is scheduled to be
	 * resumed. We need to intercept it, because the kernel
	 * stackframe contains references to the parent thread's
	 * stack (which we cannot access) - and even worse,
	 * we are running from the userland(!) stack, because we
	 * currently run in kernel mode and thus esp0 is not
	 * restored.
	 *
	 * However, the top of the kernel stack will still have
	 * the stackframe set up by our syscall routine (as there
	 * is no other way to do a clone) - we can use that
	 * information to restore the caller.
	 *
	 * Note that we must not touch %eax in any way since it is
	 * used to pass the return code to the cloned thread.
	 */

	/* Switch to the process' kernel stack */
	movw	$GDT_SEL_KERNEL_PCPU, %bx
	movw	%bx, %fs
	movl	%fs:(PCPU_CONTEXT), %ebx
	movl	CTX_ESP0(%ebx), %esp

	/* And to the process page tables */
	movl	CTX_CR3(%ebx), %edx
	movl	%edx, %cr3

	/*
	 * The syscall code will have created a stackframe to
	 * pass the arguments to the syscall handler; we need to
	 * unwind it, since we'll have to restore the registers the
	 * ABI wanted us to preserve: %ebx, %esi and %edi.
	 *
	 * First, let's move our stack to where it was once the syscall was
	 * invoked, plus the stored %ebp; we need to restore that first.
	 */
	subl	$(6 * 4), %esp

	/*
	 * There is an 'push %ebp; subl $32, %esp' in the syscall handler; this
	 * means our saved registers are at %esp-36 downwards.
	 */
	movl	-36(%esp), %ebx
	movl	-40(%esp), %esi
	movl	-44(%esp), %edi
	movl	-48(%esp), %edx
	movw	%dx, %ds
	movl	-52(%esp), %edx
	movw	%dx, %es
	movl	-56(%esp), %edx
	movw	%dx, %fs

	/* Finally, restore our %ebp and return to the new thread */
	pop	%ebp
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
