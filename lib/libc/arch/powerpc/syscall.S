#define SYSCALL(x)		\
	li	%r0, x;		\
	sc;			\
	blr

.text

#include "../../gen/syscalls.inc.S"
