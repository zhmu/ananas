/*
 * Low-level system call code.
 *
 */
.text

#include "../../../include/i386/interrupts.h"

.global syscall0, syscall1, syscall2, syscall3, syscall4, syscall5

syscall5:
	movl	24(%esp), %edi
syscall4:
	movl	20(%esp), %esi
syscall3:
	movl	16(%esp), %edx
syscall2:
	movl	12(%esp), %ecx
syscall1:
	movl	8(%esp), %ebx
syscall0:
	movl	4(%esp), %eax
	int	$SYSCALL_INT
	ret
