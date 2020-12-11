/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <ananas/stat.h>
#include <ananas/syscalls.h>
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "../sys/syscall.h"

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
    auto& curThread = thread::GetCurrent();

    const Result result = perform_syscall(&curThread, a);
    return result.AsStatusCode();
}
