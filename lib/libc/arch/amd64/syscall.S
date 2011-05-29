/* Note that we must store %rcx because it will hold the return %rip */
#define SYSCALL(x)		\
	movq	$x, %rax;	\
	movq	%rcx, %r10;	\
	syscall;		\
	ret

.text

#include "../../gen/syscalls.inc.S"
