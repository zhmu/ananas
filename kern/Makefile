R?=	..

target:	syscalls.in gen_syscalls.sh
	./gen_syscalls.sh syscalls.in $R/libc/gen/syscalls.inc.c $R/include/syscalls.h $R/kernel/kern/syscalls.inc.c

clean:
	rm -f $R/libc/gen/syscalls.inc.c $R/include/syscalls.h $R/kernel/kern/syscalls.inc.c
