#include <ananas/errno.h>
#include <ananas/stat.h>
#include <ananas/syscalls.h>
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "../sys/syscall.h"

TRACE_SETUP;

static Result perform_syscall(Thread* curthread, struct SYSCALL_ARGS* a)
{
    switch (a->number) {
#include "_gen/syscalls.inc.h"
        default:
            kprintf("warning: unsupported syscall %u\n", a->number);
            return RESULT_MAKE_FAILURE(ENOSYS);
    }
}

register_t syscall(struct SYSCALL_ARGS* a)
{
    Thread* curthread = PCPU_GET(curthread);

    Result result = perform_syscall(curthread, a);

    if (result.IsFailure())
        TRACE(SYSCALL, WARN, "result=%u", result.AsStatusCode());
    return result.AsStatusCode();
}

/* vim:set ts=2 sw=2: */
