#include <ananas/syscall.h>
#include <ananas/error.h>
#include <ananas/stat.h>
#include <ananas/handle.h>
#include <ananas/lib.h>
#include <ananas/pcpu.h>
#include <ananas/syscalls.h>
#include <ananas/trace.h>

TRACE_SETUP;

static register_t
perform_syscall(thread_t* curthread, struct SYSCALL_ARGS* a)
{
	switch(a->number) {
#include "syscalls.inc.cpp"
		case 0x42:
			kprintf("[%u]", PCPU_GET(cpuid));
			reschedule();
			return ANANAS_ERROR(BAD_SYSCALL);
		default:
			kprintf("warning: unsupported syscall %u\n", a->number);
			return ANANAS_ERROR(BAD_SYSCALL);
	}
}

register_t
syscall(struct SYSCALL_ARGS* a)
{
	thread_t* curthread = PCPU_GET(curthread);

	errorcode_t err = perform_syscall(curthread, a);

	if (ananas_is_failure(err))
		TRACE(SYSCALL, WARN, "err=%u", err);
	return err;
}

/* vim:set ts=2 sw=2: */
