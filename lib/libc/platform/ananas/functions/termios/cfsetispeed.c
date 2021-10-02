/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <termios.h>

int cfsetispeed(struct termios* termios_p, speed_t speed)
{
    termios_p->c_ispeed = speed;
    return 0;
}
