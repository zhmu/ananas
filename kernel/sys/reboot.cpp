/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/reboot.h>
#include "kernel/result.h"
#include "kernel/shutdown.h"
#include "kernel/trace.h"
#include "syscall.h"

TRACE_SETUP;

Result sys_reboot(Thread* t, int how)
{
    TRACE(SYSCALL, FUNC, "t=%p, how=%d", t, how);

    shutdown::ShutdownType type;
    switch (how) {
        case ANANAS_REBOOT_HALT:
            type = shutdown::ShutdownType::Halt;
            break;
        case ANANAS_REBOOT_REBOOT:
            type = shutdown::ShutdownType::Reboot;
            break;
        case ANANAS_REBOOT_POWERDOWN:
            type = shutdown::ShutdownType::PowerDown;
            break;
        default:
            return RESULT_MAKE_FAILURE(EINVAL);
    }
    shutdown::RequestShutdown(type);
    return Result::Success();
}
