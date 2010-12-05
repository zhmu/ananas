R?=	..

AWK?=	awk

target:	syscalls.in gen_syscalls.sh
	AWK=${AWK} ./gen_syscalls.sh syscalls.in $R/lib/libc/gen/syscalls.inc.S $R/include/_gen/syscalls.h $R/kernel/kern/syscalls.inc.c

clean:
	rm -f $R/lib/libc/gen/syscalls.inc.c $R/include/_gen/syscalls.h $R/kernel/kern/syscalls.inc.c
