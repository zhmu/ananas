/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/thread.h"

void sys_exit(Thread* t, int exitcode)
{
    t->Terminate(THREAD_MAKE_EXITCODE(THREAD_TERM_SYSCALL, exitcode));
}
