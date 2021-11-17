/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <errno.h>
#include "_todo.h"

long pathconf(const char* path, int name)
{
    TODO;
    errno = EINVAL;
    return -1;
}
