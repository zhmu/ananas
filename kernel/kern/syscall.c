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

	errorcode_t err = perform_syscall(curthread, a);

	if (err != ANANAS_ERROR_NONE)
		TRACE(SYSCALL, WARN, "err=%u", err);
	return err;
}

/* vim:set ts=2 sw=2: */
