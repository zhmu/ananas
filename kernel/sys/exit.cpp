/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/thread.h"
#include <ananas/wait.h>

void sys_exit(int exitcode)
{
    auto& t = thread::GetCurrent();
    t.Terminate(W_MAKE(W_STATUS_EXITED, exitcode & 0xff));
}
