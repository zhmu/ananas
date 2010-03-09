/*
 * Low-level system call code.
 *
 */
.text

#include "machine/interrupts.h"

.global syscall0, syscall1, syscall2, syscall3, syscall4, syscall5

/*
syscall5:
	movl	20(%esp), %edi
syscall4:
	movl	16(%esp), %esi
 */

syscall3:
	pushl	%edx
	pushl	%ecx
	pushl	%ebx

	movl	24(%esp), %edx
	movl	20(%esp), %ecx
	movl	16(%esp), %ebx
	int	$SYSCALL_INT

	popl	%ebx
	popl	%ecx
	popl	%edx
	ret

syscall2:
	pushl	%ecx
	pushl	%ebx

	movl	16(%esp), %ecx
	movl	12(%esp), %ebx
	int	$SYSCALL_INT

	popl	%ebx
	popl	%ecx
	ret

syscall1:
	pushl	%ebx

	movl	8(%esp), %ebx
	int	$SYSCALL_INT

	popl	%ebx
	ret

syscall0:
	int	$SYSCALL_INT
	ret
