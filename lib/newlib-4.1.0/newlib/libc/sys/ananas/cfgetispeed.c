/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <termios.h>

speed_t cfgetispeed(const struct termios* termios_p)
{
    return termios_p->c_ispeed;
}
