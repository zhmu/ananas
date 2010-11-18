/*
 * Stub called immediately after launch.
 */
.text

.globl _start
.extern cstart

_start:
	/* threadinfo should be passed in %rsi, which we won't touch */
	call	cstart

	/* main returned - call the exit syscall with the result code */
	movq	%rax, %rdi
	call	sys_exit
