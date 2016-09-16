#include "machine/interrupts.h"

#define SYSCALL(x)		\
	movl	$x, %eax;	\
	int	$SYSCALL_INT;	\
	ret

.text

#include "../../gen/syscalls.inc.S"
