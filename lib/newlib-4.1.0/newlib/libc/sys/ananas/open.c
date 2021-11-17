/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include "_map_statuscode.h"

int open(const char* filename, int mode, ...)
{
    int perm = 0;
    if (mode & O_CREAT) {
        va_list va;
        va_start(va, mode);
        perm = va_arg(va, unsigned int);
        va_end(va);
    }
    return _open(filename, mode, perm);
}
