/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <sys/tty.h>
#include <sys/ioctl.h>
#include <termios.h>

int tcdrain(int fildes)
{
    return ioctl(fildes, TIOCDRAIN, 0);
}
