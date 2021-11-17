/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <ananas/tty.h>
#include <sys/ioctl.h>
#include <errno.h>

int _isatty(int fildes)
{
    char s[32] = { 0 };
    ioctl(fildes, TIOGDNAME, s, sizeof(s));
    if (s[0] != '\0') return 1;
    errno = ENOTTY;
    return 0;
}
