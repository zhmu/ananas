#include <sys/syscall.h>
#include <sys/lib.h>
#include <sys/pcpu.h>
#include <syscalls.h>

register_t
syscall(struct SYSCALL_ARGS* a)
{
	switch(a->number) {
#include "syscalls.inc.c"
		default:
			kprintf("syscall, CPU=%u, no=%x,a1=%u,a2=%u,a3=%u,a4=%u,a5=%u\n",
			 PCPU_GET(cpuid),
			 a->number, a->arg1, a->arg2, a->arg3, a->arg4, a->arg5);
			break;
	}

	return 0xf00db00b;
}

/* vim:set ts=2 sw=2: */
