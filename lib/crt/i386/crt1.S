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
	pushl	%eax
	pushl	%eax		/* XXX to avoid bug in int30 code */
	pushl	%eax
	pushl	%eax
	pushl	%eax
	pushl	%eax
	call	sys_exit
