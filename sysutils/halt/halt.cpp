/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/statuscode.h>
#include <ananas/syscalls.h>
#include <ananas/reboot.h>
#include <errno.h>
#include <stdio.h>

extern "C" {
#include <ananas/syscalls.h>
}

namespace
{
    int reboot(int how)
    {
        statuscode_t result = sys_reboot(how);
        if (ananas_statuscode_is_success(result))
            return 0;
        errno = ananas_statuscode_extract_errno(result);
        return -1;
    }

} // unnamed namespace

int main(int argc, char* argv[])
{
    int rc = reboot(ANANAS_REBOOT_HALT);
    if (rc < 0)
        perror("reboot");
    return rc;
}
