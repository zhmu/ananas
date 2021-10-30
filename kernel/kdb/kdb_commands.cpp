/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel-md/md.h"

namespace
{
    const kdb::RegisterCommand kdbReboot("reboot", "Force a reboot", [](int, const kdb::Argument*) {
        md::Reboot();
    });

} // unnamed namespace
