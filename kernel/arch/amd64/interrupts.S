/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19
.globl irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9, irq10 
.globl irq11, irq12, irq13, irq14, irq15, syscall_handler
.globl clone_return
.globl userland_trampoline, kthread_trampoline

#include <machine/vm.h>
#include <machine/param.h>
#include <machine/macro.h>
#include "options.h"
#include "asmsyms.h"

#define SAVE_REGISTERS \
	movq	%rax, SF_RAX(%rsp); \
	movq	%rbx, SF_RBX(%rsp); \
	movq	%rcx, SF_RCX(%rsp); \
	movq	%rdx, SF_RDX(%rsp); \
	movq	%rbp, SF_RBP(%rsp); \
	movq	%rsi, SF_RSI(%rsp); \
	movq	%rdi, SF_RDI(%rsp); \
	movq	%rsp, SF_RSP(%rsp); \
	movq	%r8,  SF_R8 (%rsp); \
	movq	%r9,  SF_R9 (%rsp); \
	movq	%r10, SF_R10(%rsp); \
	movq	%r11, SF_R11(%rsp); \
	movq	%r12, SF_R12(%rsp); \
	movq	%r13, SF_R13(%rsp); \
	movq	%r14, SF_R14(%rsp); \
	movq	%r15, SF_R15(%rsp)

#define RESTORE_REGISTERS \
	movq	SF_RAX(%rsp), %rax; \
	movq	SF_RBX(%rsp), %rbx; \
	movq	SF_RCX(%rsp), %rcx; \
	movq	SF_RDX(%rsp), %rdx; \
	movq	SF_RBP(%rsp), %rbp; \
	movq	SF_RSI(%rsp), %rsi; \
	movq	SF_RDI(%rsp), %rdi; \
	/* movq	SF_RSP(%rsp), %rsp; */ \
	movq	SF_R8 (%rsp), %r8; \
	movq	SF_R9 (%rsp), %r9; \
	movq	SF_R10(%rsp), %r10; \
	movq	SF_R11(%rsp), %r11; \
	movq	SF_R12(%rsp), %r12; \
	movq	SF_R13(%rsp), %r13; \
	movq	SF_R14(%rsp), %r14; \
	movq	SF_R15(%rsp), %r15

#define EXCEPTION_WITHOUT_ERRCODE(n) \
exception ## n: \
	subq	$SF_RIP, %rsp; \
	movq	$n, SF_TRAPNO(%rsp); \
	movq	$0, SF_ERRNUM(%rsp); \
	jmp	do_exception

#define EXCEPTION_WITH_ERRCODE(n) \
exception ## n: \
	subq	$SF_ERRNUM, %rsp; \
	movq	$n, SF_TRAPNO(%rsp); \
	jmp	do_exception

#define IRQ_HANDLER(n) \
irq ## n: \
	subq	$SF_RIP, %rsp; \
	movq	$n, SF_TRAPNO(%rsp); \
	movq	$0, SF_ERRNUM(%rsp); \
	jmp	do_irq

do_exception:
	SAVE_REGISTERS

	/* If we didn't come from the kernel, swap the %gs register */
	cmpl	$GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
	je	1f

	swapgs

1:

	/* Call the exception handler! */
	movq	%rsp, %rdi
	call	exception

do_return:
	/* Restore %gs if needed */
	cmpl	$GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
	je	1f

	swapgs

1:
	RESTORE_REGISTERS

	/* skip over the stackframe */
	addq	$SF_RIP, %rsp
	iretq

do_irq:
	SAVE_REGISTERS

	/* If we didn't come from the kernel, swap the %gs register */
	cmpl	$GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
	je	1f

	swapgs

1:	/* Increment the nested IRQ count */
	incq	%gs:(PCPU_NESTEDIRQ)

	/* Restore the interrupt flag */
	movq	SF_RFLAGS(%rsp), %rax
	testq	$0x200, %rax
	jz	1f

	sti

1:	/* Call the interrupt handler! */
	movq	%rsp, %rdi
	call	interrupt_handler

	jmp	do_return

/* Now we just need to list the exception handlers */
EXCEPTION_WITHOUT_ERRCODE(0)
EXCEPTION_WITHOUT_ERRCODE(1)
EXCEPTION_WITHOUT_ERRCODE(2) /* NMI */
EXCEPTION_WITHOUT_ERRCODE(3)
EXCEPTION_WITHOUT_ERRCODE(4)
EXCEPTION_WITHOUT_ERRCODE(5)
EXCEPTION_WITHOUT_ERRCODE(6)
EXCEPTION_WITHOUT_ERRCODE(7)
EXCEPTION_WITH_ERRCODE(8)
EXCEPTION_WITHOUT_ERRCODE(9)
EXCEPTION_WITH_ERRCODE(10)
EXCEPTION_WITH_ERRCODE(11)
EXCEPTION_WITH_ERRCODE(12)
EXCEPTION_WITH_ERRCODE(13)
EXCEPTION_WITH_ERRCODE(14)
EXCEPTION_WITHOUT_ERRCODE(16)
EXCEPTION_WITH_ERRCODE(17)
EXCEPTION_WITHOUT_ERRCODE(18)
EXCEPTION_WITHOUT_ERRCODE(19)

/* And the interrupts */
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

/*
 * System call handler - will be called using SYSCALL; only %cs/%ss will be set
 * up. We use the same calling convention as Linux, as outlined in the System V
 * ABI AMD64 specification 0.99, section A.2.1.
 *
 * On syscall, %rcx is set to the userland %rip and %r11 are the original flags.
 */
syscall_handler:
	swapgs

	/* Store the userland's %rsp in PCPU register and load our intended %rsp */
	movq	%rsp, %gs:PCPU_SYSCALLRSP
	movq	%gs:PCPU_RSP0, %rsp

	/* Create a stack frame and store the syscall arguments */
	subq	$SYSARG_SIZE, %rsp
	movq	%rax, SYSARG_NUM(%rsp)
	movq	%rdi, SYSARG_ARG1(%rsp)
	movq	%rsi, SYSARG_ARG2(%rsp)
	movq	%rdx, SYSARG_ARG3(%rsp)
	/* %rcx will be stored in %r10 because SYSCALL uses it */
	movq	%r10, SYSARG_ARG4(%rsp)
	movq	%r8,  SYSARG_ARG5(%rsp)
	/* movq	%r9,  SYSARG_ARG6(%rsp) */

	/* Store the argument for syscall() later on */
	movq	%rsp, %rdi

	pushq	%gs:PCPU_SYSCALLRSP	/* userland %rsp */
	pushq	%rcx		/* return %rip */
	pushq	%r11		/* return %rflags */
	/* Store the registers required by the ABI spec */
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	/* XXX We should re-enable interrupts here */

	/* Invoke syscall - note that this uses %rdi set above */
	call	syscall

	/*
	 * No interrupts while restoring - we'll return the original %rflags
	 * which will enable them as needed.
	 */
	cli

	/* Restore the required registers */
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	/* Restore %r11/%rcx (used by sysret) and fix the stack */
	popq	%r11
	popq	%rcx
	popq	%rsp

	swapgs
	sysretq

clone_return:
	/*
	 * This function will be called in the cloned thread;
	 * the purpose is to dissect the kernel stack and
	 * return to the syscall-invoking thread.
	 *
	 * Note that the md_thread_clone() function will already
	 * have updated the stackpointer to protect our saved
	 * registers; we must take the offsets into account.
	 *
	 * %rax must not be touched here; it's the return value
	 * of clone and thus must be left unharmed.
	 */
	hlt

	sysretq

kthread_trampoline:
	/* release our previous thread (will be in %rbx, see md_thread_switch) */
	movq	%rbx, %rdi
	call	scheduler_release

	/* Fetch the arguments: arg1 is %rip and %arg2 is the argument */
	movq	%gs:(PCPU_CURTHREAD), %rbx
	movq	T_ARG1(%rbx), %rax		/* %rip */
	movq	T_ARG2(%rbx), %rdi		/* argument */

	pushq	$1f
	pushq	$0x200				/* Interrupt Flag */
	pushq	$GDT_SEL_KERNEL_CODE
	pushq	%rax
	iretq

1:
	/* kthread returned; this isn't supposed to happen */
	pushq	$0
	pushq	$0
	pushq	$0
	pushq	$kthread_panicmsg
	call	_panic

kthread_panicmsg:
	.ascii	"kthread returned"
	.byte	0

userland_trampoline:
	/* release our previous thread (will be in %rbx, see md_thread_switch) */
	movq	%rbx, %rdi
	call	scheduler_release

	/* Fetch the arguments: arg1 is %rip and %arg2 is the argument */
	movq	%gs:(PCPU_CURTHREAD), %rbx
	movq	T_ARG1(%rbx), %rax		/* %rip */
	movq	T_ARG2(%rbx), %rdi		/* argument */

	/* Set up %ds/%es */
	movq	$(GDT_SEL_USER_DATA + 3), %rdx
	movw	%dx, %ds
	movw	%dx, %es

	pushq	$(GDT_SEL_USER_DATA + 3)			/* ss */
	pushq	$(USERLAND_STACK_ADDR + THREAD_STACK_SIZE)	/* rsp */
	pushq	$0x200					/* eflags */
	pushq	$(GDT_SEL_USER_CODE + 3)		/* cs */
	pushq	%rax					/* rip */
	iretq
