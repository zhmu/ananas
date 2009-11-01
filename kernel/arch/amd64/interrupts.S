/*
 * Low-level assembly code to pass an interrupt to a higher-level handler.
 */
.text
.globl exception0, exception1, exception2, exception3, exception4, exception5
.globl exception6, exception7, exception8, exception9, exception10, exception11
.globl exception12, exception13, exception14, exception16, exception17
.globl exception18, exception19

#include "machine/vm.h"
#include "options.h"
#include "param.h"
#include "asmsyms.h"

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

do_exception:
	/* Store all registers */
	movq	%rax, SF_RAX(%rsp)
	movq	%rbx, SF_RBX(%rsp)
	movq	%rcx, SF_RCX(%rsp)
	movq	%rdx, SF_RDX(%rsp)
	movq	%rbp, SF_RBP(%rsp)
	movq	%rsi, SF_RSI(%rsp)
	movq	%rdi, SF_RDI(%rsp)
	movq	%rsp, SF_RSP(%rsp)
	movq	%r8,  SF_R8 (%rsp)
	movq	%r9,  SF_R9 (%rsp)
	movq	%r10, SF_R10(%rsp)
	movq	%r11, SF_R11(%rsp)
	movq	%r12, SF_R12(%rsp)
	movq	%r13, SF_R13(%rsp)
	movq	%r14, SF_R14(%rsp)
	movq	%r15, SF_R15(%rsp)

	/* Call the exception handler! */
	movq	%rsp, %rdi
	call	exception

do_return:
	movq	SF_RAX(%rsp), %rax
	movq	SF_RBX(%rsp), %rbx
	movq	SF_RCX(%rsp), %rcx
	movq	SF_RDX(%rsp), %rdx
	movq	SF_RBP(%rsp), %rbp
	movq	SF_RSI(%rsp), %rsi
	movq	SF_RDI(%rsp), %rdi
	movq	SF_RSP(%rsp), %rsp
	movq	SF_R8 (%rsp), %r8
	movq	SF_R9 (%rsp), %r9
	movq	SF_R10(%rsp), %r10
	movq	SF_R11(%rsp), %r11
	movq	SF_R12(%rsp), %r12
	movq	SF_R13(%rsp), %r13
	movq	SF_R14(%rsp), %r14
	movq	SF_R15(%rsp), %r15

	/* skip over the stackframe */
	addq	$SF_RIP, %rsp
	iretq

	
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
