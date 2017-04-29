R?=	..

AWK?=	awk

target:	syscalls.in gen_syscalls.sh
	mkdir -p $R/include/_gen
	mkdir -p $R/lib/libc/gen
	AWK=${AWK} ./gen_syscalls.sh syscalls.in $R/lib/libc/gen/syscalls.inc.S $R/include/_gen/syscalls.h $R/kernel/kern/syscalls.inc.cpp

clean:
	rm -f $R/lib/libc/gen/syscalls.inc.S $R/include/_gen/syscalls.h $R/kernel/kern/syscalls.inc.cpp
