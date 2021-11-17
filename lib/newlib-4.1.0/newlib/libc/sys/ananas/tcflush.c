/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <ananas/tty.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>

int tcflush(int fildes, int queue_selector)
{
    switch(queue_selector)
    {
        case TCIFLUSH:
            return ioctl(fildes, TIOCFLUSHR, 0);
        case TCOFLUSH:
            return ioctl(fildes, TIOCFLUSHW, 0);
        case TCIOFLUSH:
            return ioctl(fildes, TIOCFLUSHRW, 0);
    }

    errno = EINVAL;
    return -1;
}
