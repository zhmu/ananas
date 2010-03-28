/*
 * Stub called immediately after launch.
 */
.text

.globl _start
.extern cstart, sys_exit

_start:
	/* XXX argc, argv */
	pushl	$0x0
	pushl	$0x0
	call	cstart

	/* main finished; pass it's exit code to the exit syscall */
	call	sys_exit

