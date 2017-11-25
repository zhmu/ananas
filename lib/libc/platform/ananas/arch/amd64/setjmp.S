/*
 * Implements archeic setjmp/longjmp functions. Based on FreeBSD's
 * lib/libc/amd64/gen/_setjmp.S.
 */
.text

.global setjmp, longjmp

/*
 * setjmp(jmp_buf env): store context for a longjmp.
 */
.type setjmp, @function
setjmp:
	movq	0(%rsp), %rdx	/* get return address from stack */

	movq	%rdx,  0(%rdi)	/* store return address */
	movq	%rbx,  8(%rdi)	/* store rbx */
	movq	%rsp, 16(%rdi)	/* store rsp */
	movq	%rbp, 24(%rdi)	/* store rbp */
	movq	%r12, 32(%rdi)	/* store r12 */
	movq	%r13, 40(%rdi)	/* store r13 */
	movq	%r14, 48(%rdi)	/* store r14 */
	movq	%r15, 56(%rdi)	/* store r15 */

	xorq	%rax, %rax
	ret

/* longjmp(jmp_buf env, int val): restore context previously set using setjmp */
.type longjmp, @function
longjmp:
	movq	%rdi, %rax	/* get return value */

	movq	 0(%rdi), %rcx	/* return address */
	movq	 8(%rdi), %rbx	/* restore rbx */
	movq	16(%rdi), %rsp	/* restore rsp */
	movq	24(%rdi), %rbp	/* restore rbp */
	movq	32(%rdi), %r12	/* restore r12 */
	movq	40(%rdi), %r13	/* restore r13 */
	movq	48(%rdi), %r14	/* restore r14 */
	movq	56(%rdi), %r15	/* restore r15 */

	movq	%rcx, 0(%rsp)	/* overwrite return address ... */
	ret			/* ... and jump to it */
