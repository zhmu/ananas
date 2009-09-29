/*
 * Stub called immediately after launch.
 */
.text

.globl _start

_start:
	/* XXX argc, argv */
	pushl	$0x0
	pushl	$0x0
	call	main
	addl	$8, %esp

	pushl	$0x0		/* XXX exit */
	call	syscall0
	/* NOTREACHED */
