/*
 * Implements archeic setjmp/longjmp functions. Based on FreeBSD's
 * lib/libc/powerpc/gen/_setjmp.S.
 */
.text

.global setjmp, longjmp

/*
 * setjmp(jmp_buf env): store context for a longjmp.
 *
 * On the PowerPC, this is actually quite convenient as the return
 * address is stored in %lr.
 */
setjmp:
	mflr	%r5
	stw	%r5, 0(%r3)		/* store %lr */
	stw	%r1, 4(%r3)		/* store %r1 - stack pointer */
	li	%r3, 0			/* return 0 */
	blr

/* longjmp(jmp_buf env, int val): restore context previously set using setjmp */
longjmp:
	lwz	%r1, 4(%r3)		/* obtain stored %r1 */
	lwz	%r5, 0(%r3)		/* obtain stored %lr */
	mtlr	%r5
	blr
