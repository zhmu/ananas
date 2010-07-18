/*
 * Stub called immediately after launch.
 */
.text

.globl _start
.extern cstart

_start:
	/* XXX argc, argv */
	li	%r3, 0
	li	%r4, 4
	bl	cstart

	/* main returned - call the exit syscall with the result code */
	li	%r3, 0
	sc
