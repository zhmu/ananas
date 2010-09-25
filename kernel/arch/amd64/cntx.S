.text

#include <sys/types.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <machine/macro.h>
#include "asmsyms.h"

.global md_store_ctx, md_restore_ctx, md_reschedule, md_idle_thread

/*
 * Restores the thread context and jumps to the thread
 */
md_restore_ctx:
	movq	%gs:PCPU_CTX, %rsi

	/*
	 * Check if we are about to return to a rescheduler thread; if this is
	 * the case, we need to restore far less context.
	 */
	cmpq	$GDT_SEL_USER_CODE, SF_CS(%rsi)
	jne	md_restore_ctx2
	cmpq	$reschedule_back, SF_RIP(%rsi)
	je	md_restore_ctx_kernel

md_restore_ctx2:
	/*
	 * Restore as much as we can from our context.
	 */
	movq	SF_RBX(%rsi), %rbx
	movq	SF_RCX(%rsi), %rcx
	movq	SF_RDX(%rsi), %rdx
	movq	SF_RBP(%rsi), %rbp
	movq	SF_RDI(%rsi), %rdi
	movq	SF_R8(%rsi),  %r8
	movq	SF_R9(%rsi),  %r9
	movq	SF_R10(%rsi), %r10
	movq	SF_R11(%rsi), %r11
	movq	SF_R12(%rsi), %r12
	movq	SF_R13(%rsi), %r13
	movq	SF_R14(%rsi), %r14
	movq	SF_R15(%rsi), %r15

	/*
	 * Go to the new kernel stack; we need to do this, because the new
	 * thread will not have the old kernel stack mapped.
	 */
	movq	SF_SP(%rsi), %rsp

	/* Place necessary things on the stack for iretq */
	pushq	SF_SS(%rsi)
	pushq	SF_RSP(%rsi)
	pushq	SF_RFLAGS(%rsi)
	pushq	SF_CS(%rsi)
	pushq	SF_RIP(%rsi)

	/* From this point on, only rsi, rax and flags need to go */
	pushq	SF_RSI(%rsi)
	pushq	SF_RAX(%rsi)

	/* Restore the page table */
	movq	CTX_PML4(%rsi), %rax
	movq	%rax, %cr3

	/* Set ds to our new data segment */
	movw	$GDT_SEL_USER_DATA + 3, %ax
	movw	%ax, %ds

	/*
	 * Enable the task-switched flag; this will cause an exception to be
	 * thrown for every FPU instruction. We use this to restore the
	 * adequate context.
	 */
	movq	%cr0, %rax
	orq	$CR0_TS, %rax
	movq	%rax, %cr0

	/* Restore %rax/%rsi and activate the thread's new context */
	popq	%rax
	popq	%rsi
	iretq

md_restore_ctx_kernel:
	/* Restore the registers we saved before */
	movq	SF_RBP(%rsi), %rbp
	movq	SF_RBX(%rsi), %rbx
	movq	SF_R12(%rsi), %r12
	movq	SF_R13(%rsi), %r13
	movq	SF_R14(%rsi), %r14
	movq	SF_R15(%rsi), %r15

	/* Restore the stack pointer, and jump to the previous address */
	movq	SF_RSP(%rsi), %rsp
	movq	SF_RIP(%rsi), %rax
	jmp	*%rax

md_reschedule:
	/*
	 * According to the System V ABI AMD64 specification, section A.2.1,
	 * %rbp, %rbx, %r12 - %r15 'belong' to the calling function and the
	 * called function must preserve their values.
	 *
	 * Note that we don't need 'swapgs' here because this function will
	 * always be called in kernel context. Thus, we add an exception to
	 * md_restore_ctx() which causes only these registers to be restored.
	 */
	movq	%gs:PCPU_CTX, %rsi
	movq	%rbp, SF_RBP(%rsi)
	movq	%rbx, SF_RBX(%rsi)
	movq	%r12, SF_R12(%rsi)
	movq	%r13, SF_R13(%rsi)
	movq	%r14, SF_R14(%rsi)
	movq	%r15, SF_R15(%rsi)
	movq	%rsp, SF_RSP(%rsi)

	/*
	 * Now, insert our entry point; this is what will be executed once the function
	 * returns.
	 */
	movq	$GDT_SEL_USER_CODE, SF_CS(%rsi)
	movq	$reschedule_back, SF_RIP(%rsi)

	/* Throw the switch! */
	call	schedule

reschedule_back:
	/* We are back from a scheduler trip, and must return to the caller */
	ret

/*
 * This is our idle thread; it just waits for an interrupt.
 */
md_idle_thread:
	hlt
	jmp	md_idle_thread
