/*
 * Stub called immediately after launch.
 */
.text

.globl _start

_start:
	/* XXX argc, argv */
	xorq	%rsi, %rsi
	xorq	%rdi, %rdi
	call	main

	movq	$0, %rax		/* XXX exit */
	call	syscall0
	/* NOTREACHED */
