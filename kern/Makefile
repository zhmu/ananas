R?=	..

AWK?=	awk

target:	syscalls.in gen_syscalls.sh
	mkdir -p $R/include/_gen
	AWK=${AWK} ./gen_syscalls.sh syscalls.in $R/lib/libc/gen/syscalls.inc.S $R/include/_gen/syscalls.h $R/kernel/kern/syscalls.inc.c

clean:
	rm -f $R/lib/libc/gen/syscalls.inc.S $R/include/_gen/syscalls.h $R/kernel/kern/syscalls.inc.c
