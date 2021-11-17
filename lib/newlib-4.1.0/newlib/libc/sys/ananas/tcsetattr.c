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
#include <errno.h>

int tcsetattr(int fildes, int optional_actions, const struct termios* termios_p)
{
    switch(optional_actions) {
        case TCSANOW:
            return ioctl(fildes, TIOCSETA, termios_p);
        case TCSADRAIN:
            return ioctl(fildes, TIOCSETW, termios_p);
        case TCSAFLUSH:
            return ioctl(fildes, TIOCSETWF, termios_p);
    }

    errno = EINVAL;
    return -1;
}
