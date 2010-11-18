#include <ananas/syscall.h>
#include <ananas/error.h>
#include <ananas/stat.h>
#include <ananas/handle.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/syscalls.h>
#include <ananas/trace.h>

TRACE_SETUP;

#undef TRACE_SYSCALLS

static register_t
perform_syscall(thread_t curthread, struct SYSCALL_ARGS* a)
{
	switch(a->number) {
#include "syscalls.inc.c"
		default:
			kprintf("warning: unsupported syscall %u\n", a->number);
			return ANANAS_ERROR(BAD_SYSCALL);
	}
}

register_t
syscall(struct SYSCALL_ARGS* a)
{
	struct THREAD* curthread = PCPU_GET(curthread);

#ifdef TRACE_SYSCALLS
	kprintf("syscall, t=%x, no=%u,a1=0x%x,a2=0x%x,a3=0x%x,a4=0x%x,a5=0x%x ",
		curthread, a->number, a->arg1, a->arg2, a->arg3, a->arg4, a->arg5);
#endif

	errorcode_t err = perform_syscall(curthread, a);
#ifdef TRACE_SYSCALLS
	kprintf(" => syscall result %i\n", err);
#endif
	return err;
}

/* vim:set ts=2 sw=2: */
