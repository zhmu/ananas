.text

#include "asmsyms.h"
#include "param.h"
#include "types.h"
#include "machine/vm.h"
#include "machine/macro.h"

.global md_store_ctx, md_restore_ctx

/*
 * Restores the thread context and jumps to the thread
 */
md_restore_ctx:
	/*
	 * Restore as much as we can from our context.
	 */
	movq	%gs:PCPU_CTX, %rsi
	movq	SF_RAX(%rsi), %rax
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
	pushq	$GDT_SEL_USER_DATA + 3
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

	/* Restore %rax/%rsi and activate the thread's new context */
	popq	%rax
	popq	%rsi
	iretq
