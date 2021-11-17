/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <errno.h>

int getentropy(void *buffer, size_t length)
{
    errno = ENOSYS;
    return -1;
}

