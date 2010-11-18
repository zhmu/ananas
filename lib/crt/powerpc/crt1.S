/*
 * Stub called immediately after launch.
 */
.text

.globl _start
.extern cstart

_start:
	/* Construct a stackframe; return address is zero */
	li	%r0, 0
	stwu	%r1, -16(%r1)
	stw	%r0,  20(%r1)		/* LR */
	stw	%r0,  12(%r1)		/* Backchain */
	mr	%r31, %r1

	/* Call the startup code; argument is in %r3 which we didn't touch */
	bl	cstart

	/* main returned - call the exit syscall with the result code */
	li	%r0, 0
	sc

	/* NOTREACHED */
