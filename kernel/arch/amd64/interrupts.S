/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19
.globl irq1, irq2, irq3, irq4, irq5, irq6, irq7, irq8, irq9, irq10 
.globl irq11, irq12, irq13, irq14, irq15, scheduler_irq, syscall_handler

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

#define IRQ(n) \
irq ## n: \
	subq	$SF_RIP, %rsp; \
	movq	$n, SF_TRAPNO(%rsp); \
	movq	$0, SF_ERRNUM(%rsp); \
	jmp	do_interrupt

/* EOI's the interrupt vector in %rbx */
eoi:	movb	$0x20, %al
	outb	%al, $0x20
	cmp	$0x8, %rbx
	jl	skip_pic2;
	outb	%al, $0xa0
skip_pic2:
	ret

do_exception:
	SAVE_REGISTERS

	/* If we didn't come from the kernel, swap the %gs register */
	cmpl	$GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
	je	exc_swapgs1

	swapgs

exc_swapgs1:
	/* Restore the page map */
	movq	%cr3, %rax
	pushq	%rax
	movq	pml4, %rbx
	andq	$0x0fffffff, %rbx
	cmp	%rax, %rbx
	je	exc_skip_pt

	movq	%rbx, %cr3

exc_skip_pt:

	/* Call the exception handler! */
	movq	%rsp, %rdi
	addq	$8, %rdi	/* Adjust for saved %cr3 */
	call	exception

do_return:
	/* Restore %gs if needed */
	cmpl	$GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
	je	exc_swapgs2

	swapgs

exc_swapgs2:
	/* Restore the original pagetable, if it has changed */
	popq	%rbx
	movq	%cr3, %rax
	cmp	%rax, %rbx
	je	int_skiprest_pt

	movq	%rbx, %cr3

int_skiprest_pt:
	RESTORE_REGISTERS

	/* skip over the stackframe */
	addq	$SF_RIP, %rsp
	iretq

do_interrupt:
	SAVE_REGISTERS

	/* If we didn't come from the kernel, swap the %gs register */
	cmpl	$GDT_SEL_KERNEL_CODE, SF_CS(%rsp)
	je	int_swapgs

	swapgs

int_swapgs:
	/* Restore the kernel page map */
	movq	%cr3, %rax
	pushq	%rax
	movq	pml4, %rbx
	andq	$0x0fffffff, %rbx
	cmp	%rax, %rbx
	je	int_skip_pt

	movq	%rbx, %cr3

int_skip_pt:

	/* Call the interrupt handler! */
	movq	%rsp, %rdi
	addq	$8, %rdi	/* skip stored pagetable */
	call	interrupt

	/* End-Of-Interrupt the interrupt */
	movq	SF_TRAPNO(%rsp), %rbx
	call	eoi

	jmp	do_return

/*
 * Scheduler interrupt is magic - it needs to store the context to the current-context
 * pointer in the per-cpu part.
 *
 * However, we cheat and store the context on the stack initially; if we decide that it
 * has to be stored on the thread, we copy it over.
 *
 * Note that the scheduler is effectively disabled using the 'scheduler_active'
 * global variable - we need this during early startup when we are probing
 * devices (as interrupts will be enabled)
 */
scheduler_irq:
	/*
	 * See if there is a current CPU context; if not, the scheduler isn't
	 * active yet and we just need to return.
	 */
	pushq	%rax
	movl	%cs, %eax
	cmpl	$GDT_SEL_KERNEL_CODE, %eax
	je	scheduler_kern

	/* Activate kernel %gs */
	swapgs

scheduler_kern:
	/* Increment per-cpu tick count */
	incl	%gs:(PCPU_TICKCOUNT)

	/*
	 * Per-CPU data is available. See if we have a CPU context, bail
	 * out if not - note that we hand it the saved %rax so that it
	 * can restore %gs if needed.
	 */
	cmpl	$0, %gs:PCPU_CTX
	jz	scheduler_cancel
	popq	%rax

	subq	$SF_RIP, %rsp
	SAVE_REGISTERS

	/*
	 * This is just annoying: the scheduler interrupt does not really
	 * return (it just calls 'schedule', which in turn calls
	 * 'md_set_contex' to return us to the correct thread context).
	 *
	 * This means we have to force the thread's %rsp in shape again,
	 * because the thread will just have to believe nothing happened it
	 * it's happily continueing running, without any sudden stuff on their
	 * stack.
	 * 
	 * Because the interrupt is running in supervisor mode, SF_SP(%rsp)
	 * contains the userland stackpointer and likewise, SF_RSP(%rsp)
	 * contains the kernel one!
	 *
	 * We need to exchange them, but the kernel stackpointer must be
	 * updated - this is because the CPU will have pushed (in order)
	 * ss, rsp, rflags, cs, rip on there, which it expects to restore
	 * on the interrupts 'iretq'. However, since we never actually quite
	 * do the 'iretq', we must adjust the kernel stack ourselves, or
	 * just remove the 5 registers (which take up 5 * 8 = 40 bytes)
	 */
	movq	SF_SP(%rsp), %rax
	movq	%rax, SF_RSP(%rsp)
	movq	%rsp, %rax
	addq	$SF_RIP + 40, %rax
	movq	%rax, SF_SP(%rsp)

	/* Restore the page map */
	movq	%cr3, %rax
	movq	pml4, %rbx
	andq	$0x0fffffff, %rbx
	cmp	%rax, %rbx
	je	sched_skip_pt

	movq	%rbx, %cr3

sched_skip_pt:
	/*
	 * If our current thread owns the FPU, we need to store the registers. This
	 * value is only non-zero if an exception was raised due to the TS flag being
	 * set, so the FPU context will only be stored if needed.
	 */
	movq	%gs:(PCPU_FPUCTX), %rbx
	orq	%rbx, %rbx
	jz	sched_skip_fpu

	fxsave	(%rbx)

	/* Disown the FPU; we have stored it */
	movq	$0, %gs:(PCPU_FPUCTX)

sched_skip_fpu:
	/* Copy the stored context to the per-cpu context */
	movq	%rsp, %rsi
	movq	%gs:(PCPU_CTX), %rdi
	movq	$SF_SIZE, %rcx
	rep	movsb

	xorq	%rbx, %rbx
	call	eoi

	/* Dear scheduler - go! */
	jmp	schedule

	/* NOTREACHED - scheduler calls md_restore_ctx */

scheduler_cancel:
	/* Acknowledge the interrupt before exiting */
	pushq	%rax
	pushq	%rbx
	xorq	%rbx, %rbx
	call	eoi
	popq	%rbx
	popq	%rax

	/* Almost done - restore the kernel gs if needed */
	cmpl	$GDT_SEL_KERNEL_CODE, %eax
	je	sched_skip_gs

	swapgs
sched_skip_gs:
	popq	%rax
	iretq

/*
 * System call handler - will be called using SYSCALL; only cs/ss will be set
 * up. We use the same calling convention as Linux, as outlined in the System V
 * ABI AMD64 specification 0.99, section A.2.1.
 */
syscall_handler:
	swapgs
	movq	%rsp, %gs:PCPU_TEMPRSP

	/* First of all, store the syscall arguments */
	subq	$SYSARG_SIZE, %rsp
	movq	%rax, SYSARG_NUM(%rsp)
	movq	%rdi, SYSARG_ARG1(%rsp)
	movq	%rsi, SYSARG_ARG2(%rsp)
	movq	%rdx, SYSARG_ARG3(%rsp)
	movq	%r10, SYSARG_ARG4(%rsp)
	movq	%r8,  SYSARG_ARG5(%rsp)
	/* movq	%r9,  SYSARG_ARG6(%rsp) */

	/* Store the argument for syscall() later on */
	movq	%rsp, %rdi

	pushq	%rcx
	/* Store the registers required by the ABI spec */
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	/* Activate the kernel pagemap */
	movq	%cr3, %rax
	pushq	%rax
	movq	pml4, %rax
	andq	$0x0fffffff, %rax
	movq	%rax, %cr3

	/* Invoke syscall - note that this uses %rdi set above */
	call	syscall

	/* Restore the old userland map */
	popq	%rbx
	movq	%rbx, %cr3

	/* Restore the required registers */
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	/* Restore %rcx (it is used by sysret) and fix the stack */
	popq	%rcx

	movq	%gs:PCPU_TEMPRSP, %rsp
	swapgs
	sysretq

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
IRQ(1)
IRQ(2)
IRQ(3)
IRQ(4)
IRQ(5)
IRQ(6)
IRQ(7)
IRQ(8)
IRQ(9)
IRQ(10)
IRQ(11)
IRQ(12)
IRQ(13)
IRQ(14)
IRQ(15)
