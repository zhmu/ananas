/*
 * Low-level system call code.
 *
 */
.text

#include "../../../include/i386/interrupts.h"

.global syscall0, syscall1, syscall2, syscall3, syscall4, syscall5

syscall5:
	movl	24(%ebp), %edi
syscall4:
	movl	20(%ebp), %esi
syscall3:
	movl	16(%ebp), %edx
syscall2:
	movl	12(%ebp), %ecx
syscall1:
	movl	8(%ebp), %ebx
syscall0:
	movl	4(%ebp), %eax
	int	$SYSCALL_INT
	ret
