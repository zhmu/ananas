#include <ananas/error.h>
#include <ananas/stat.h>
#include <ananas/syscalls.h>
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/trace.h"
#include "../sys/syscall.h"

TRACE_SETUP;

static register_t
perform_syscall(Thread* curthread, struct SYSCALL_ARGS* a)
{
	switch(a->number) {
#include "syscalls.inc.cpp"
		default:
			kprintf("warning: unsupported syscall %u\n", a->number);
			return ANANAS_ERROR(BAD_SYSCALL);
	}
}

register_t
syscall(struct SYSCALL_ARGS* a)
{
	Thread* curthread = PCPU_GET(curthread);

	errorcode_t err = perform_syscall(curthread, a);

	if (ananas_is_failure(err))
		TRACE(SYSCALL, WARN, "err=%u", err);
	return err;
}

/* vim:set ts=2 sw=2: */
