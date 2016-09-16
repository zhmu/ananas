/*
 * Implements archeic setjmp/longjmp functions. Based on FreeBSD's
 * lib/libc/i386/gen/_setjmp.S.
 */
.text

.global setjmp, longjmp

/*
 * setjmp(jmp_buf env): store context for a longjmp.
 */
setjmp:
	movl	4(%esp), %eax	/* get env pointer */
	movl	0(%esp), %edx	/* get return address from stack */

	movl	%edx,  0(%eax)	/* store return address */
	movl	%ebx,  4(%eax)	/* store ebx */
	movl	%esp,  8(%eax)	/* store esp */
	movl	%ebp, 12(%eax)	/* store ebp */
	movl	%esi, 16(%eax)	/* store esi */
	movl	%edi, 20(%eax)	/* store edi */

	xorl	%eax, %eax
	ret

/* longjmp(jmp_buf env, int val): restore context previously set using setjmp */
longjmp:
	movl	4(%esp), %edx	/* get env pointer */
	movl	8(%esp), %eax	/* get return value */

	movl	 0(%edx), %ecx	/* return address */
	movl	 4(%edx), %ebx	/* restore ebx */
	movl	 8(%edx), %esp	/* restore esp */
	movl	12(%edx), %ebp	/* restore ebp */
	movl	16(%edx), %esi	/* restore esi */
	movl	20(%edx), %edi	/* restore edi */

	movl	%ecx, 0(%esp)	/* overwrite return address ... */
	ret			/* ... and jump to it */
