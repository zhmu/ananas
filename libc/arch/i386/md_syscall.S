/*
 * Low-level system call code.
 *
 */
.text

#include "../../../include/i386/interrupts.h"

.global syscall0, syscall1, syscall2, syscall3, syscall4, syscall5

syscall5:
	movl	20(%esp), %edi
syscall4:
	movl	16(%esp), %esi
syscall3:
	movl	12(%esp), %edx
syscall2:
	movl	8(%esp), %ecx
syscall1:
	movl	4(%esp), %ebx
syscall0:
	int	$SYSCALL_INT
	ret
