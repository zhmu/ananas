/*
 * Stub called immediately after launch.
 */
.text

.globl _start
.extern cstart

_start:
	/* XXX argc, argv */
	pushq	$0x0
	pushq	$0x0
	call	cstart

	/* main returned - call the exit syscall with the result code */
	movq	%rax, %rdi
	call	sys_exit
