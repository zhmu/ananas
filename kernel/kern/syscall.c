#include "syscall.h"

register_t
syscall(struct SYSCALL_ARGS* a)
{
	kprintf("syscall, no=%x,a1=%u,a2=%u,a3=%u,a4=%u,a5=%u\n",
	 a->number, a->arg1, a->arg2, a->arg3, a->arg4, a->arg5);

	return 0xf00db00b;
}

/* vim:set ts=2 sw=2: */
