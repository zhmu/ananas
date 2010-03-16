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

