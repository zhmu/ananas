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

int tcgetattr(int fildes, struct termios* termios_p)
{
    return ioctl(fildes, TIOCGETA, termios_p);
}
