/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <ananas/tty.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>

int tcsendbreak(int fildes, int duration)
{
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = 250000000; // 0.25 sec

    if (ioctl(fildes, TIOCSBRK, 0) < 0) return -1;
    nanosleep(&tv, NULL);
    if (ioctl(fildes, TIOCCBRK, 0) < 0) return -1;
    return 0;
}
