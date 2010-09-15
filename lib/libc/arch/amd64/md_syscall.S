#include "machine/interrupts.h"

#define SYSCALL(x)		\
	movq	$x, %rax;	\
	syscall;		\
	ret

.text

#include "../../gen/syscalls.inc.S"
