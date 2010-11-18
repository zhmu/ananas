/*
 * Stub called immediately after launch.
 */
.text

.globl _start
.extern cstart, sys_exit

_start:
	push	%esi		/* threadinfo */
	call	cstart
	add	$4, %esp

	/* main finished; pass it's exit code to the exit syscall */
	call	sys_exit
