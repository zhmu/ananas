/*
 * Low-level system call code.
 *
 */
.text

#include "../../../include/i386/interrupts.h"

.global syscall0, syscall1, syscall2, syscall3, syscall4, syscall5

syscall0:
	push	%ebp
	mov	%ebp, %esp

	movl	4(%ebp), %eax
	int	$SYSCALL_INT

	pop	%ebp
	ret

