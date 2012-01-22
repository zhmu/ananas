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
.globl userland_trampoline, kthread_trampoline

#include "options.h"
#include "machine/param.h"
#include "machine/vm.h"
#include "asmsyms.h"

#define SANITY_CHECKS

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

	/* Increment the nested IRQ count */
	incl	%fs:(PCPU_NESTEDIRQ)

	/* Restore the interrupt flag */
	movl	SF_EFLAGS(%esp), %eax
	testl	$0x200, %eax
	jz	1f

	sti

1:	/* Call the interrupt handler */
	pushl	%esp
	call	interrupt_handler
	addl	$4, %esp

	/* Restore previous state */
	RESTORE_SEGS
	RESTORE_REGISTERS
	addl	$SF_EIP, %esp
	iret

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

	/* Call the exception handler */
	pushl	%esp
	call	exception_handler
	addl	$4, %esp

	/* Restore previous state */
	RESTORE_SEGS
	RESTORE_REGISTERS
	addl	$SF_EIP, %esp
	iret

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
 * System call interrupt; system call number should be placed in %eax, all
 * arguments are expected to be placed on the stack.  Note that we only
 * preserve what's needed by the System V ABI because that's what everyone
 * seems to use anyway.
 */
syscall_int:
	/*
	 * System V ABI for Intel386 Architecture says we only have to
	 * preserve %ebx, %esi, %edi, %ebp and %esp (the latter is done
	 * implicitely by the hardware).
	 */
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi

	/* Save segment registers as well; we only use ds/es/fs */
	pushl	%ds
	pushl	%es
	pushl	%fs

#ifdef SANITY_CHECKS
	/*
	 * Sanity check: a system call must be made from userland, thus %ds
	 * must be userland version
	 */
	movw	%ds, %bx
	cmpw	$(GDT_SEL_USER_DATA + 3), %bx
	je	syscall_start_dsok

	int 	$3

syscall_start_dsok:
	/* Sanity check: interrupts must be enabled when entering */
	movl	36(%esp), %ebx
	andl	$0x200, %ebx
	orl	%ebx, %ebx
	jne	syscall_start_ifok

	int	$3

syscall_start_ifok:
#endif

	/*
	 * Switch to kernel segment register context (ds/es/fs); this ensures
	 * the stores below will work (%esi uses %ds implicitely)
	 */
	movw	$GDT_SEL_KERNEL_DATA, %bx
	movw	%bx, %ds
	movw	%bx, %es
	movw	$GDT_SEL_KERNEL_PCPU, %bx
	movw	%bx, %fs

	/*
	 * Fetch the caller's stack pointer; this will be saved at our
	 * current stack - but note that we have already created a
	 * stackframe, so we just use that as base.
	 *
	 * The 40 comes from stored fs es ds edi esi ebx ebp eip cs flg esp ss
	 * as they occur on offsets 0  4  8  12  16  20  24  28 32  36  40  44
	 */
	movl 40(%esp), %esi

	/*
	 * Set up a pointer to the structure, and copy 6 arguments
	 * in place. Note that the stack grows backward, so we need
	 * to place them in reverse order.
	 */
	pushl	20(%esi)
	pushl	16(%esi)
	pushl	12(%esi)
	pushl	 8(%esi)
	pushl	 4(%esi)
	pushl	%eax

	/*
	 * Invoke the generic syscall hander; return values will be passed in
	 * %eax, so we cannot thrash that. Note that our argument is pushed
	 * beforehand, which simply points to the syscall values.
	 */
	pushl	%esp
	call	syscall
	addl	$28, %esp	/* 6 regs + esp */

syscall_return:

	/* Restore registers */
	popl	%fs
	popl	%es
	popl	%ds

#ifdef SANITY_CHECKS
	/*
	 * Sanity check: we must have restored an userland ds; if we don't,
	 * iret will reset %ds to null because it can't be accessed in ring 3,
	 * leading to hard-to-debug problems. Best to verify it here.
	 */
	movw	%ds, %bx
	cmpw	$(GDT_SEL_USER_DATA + 3), %bx
	je	syscall_end_dsok

	int	$3

syscall_end_dsok:
	/* Sanity check: interrupts must be enabled when a syscall is done */
	movl	24(%esp), %ebx
	andl	$0x200, %ebx
	orl	%ebx, %ebx
	jne	syscall_end_ifok

	int	$3

syscall_end_ifok:
#endif

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	iret

clone_return:
	/*
	 * Once here, the to-be-cloned thread is scheduled to be
	 * resumed. We are returning to a new thread, so we'll
	 * need to restore the context exactly as it were when
	 * the thread did the syscall.
	 *
	 * However, the top of the kernel stack will still have
	 * the stackframe set up by our syscall routine (as there
	 * is no other way to do a clone) - we can use that
	 * information to restore the caller.
	 *
	 * Note that the ABI dictates that %eax is used to return
	 * a value to the new thread, so we'll have to restore it.
	 */

	/* Rewind the stack as created by syscall_int */
	movl	%fs:(PCPU_CURTHREAD), %ebx
	movl	T_ESP0(%ebx), %esp
	subl	$48, %esp

	/* Fetch return value */
	movl	T_ARG1(%ebx), %eax

	/* And terminate the system call */
	jmp	syscall_return

#ifdef OPTION_SMP
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
IRQ_HANDLER(0)
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

kthread_trampoline:
	/* Fetch arguments; arg1 is the %eip and arg2 is the argument */
	movl	%fs:(PCPU_CURTHREAD), %ebx
	movl	T_ARG1(%ebx), %eax		/* eax = eip */
	pushl	T_ARG2(%ebx)

	/* Call the kthread code with interrupts enabled */
	pushl	$0x200
	pushl	$GDT_SEL_KERNEL_CODE
	pushl	%eax
	iret

userland_trampoline:
	/* Fetch arguments; arg1 is the %eip and arg2 is the argument */
	movl	%fs:(PCPU_CURTHREAD), %ebx
	movl	T_ARG1(%ebx), %eax		/* eax = eip */
	movl	T_ARG2(%ebx), %esi		/* esi = arg */

	/* Set up %ds/%es to point to the userland segments */
	movl	$(GDT_SEL_USER_DATA + 3), %edx
	movl	%dx, %ds
	movl	%dx, %es
	
	pushl	$(GDT_SEL_USER_DATA + 3)			/* ss */
	pushl	$(USERLAND_STACK_ADDR + THREAD_STACK_SIZE)	/* esp */
	pushl	$0x200					/* eflags */
	pushl	$(GDT_SEL_USER_CODE + 3)		/* cs */
	pushl	%eax					/* eip */
	iret
