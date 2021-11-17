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

static int get_flow_char(int fildes, int action, char* c)
{
    struct termios tios;
    if (tcgetattr(fildes, &tios) < 0) return -1;

    if (action == TCION) {
        *c = tios.c_cc[VSTART];
        return 0;
    }
    if (action == TCIOFF) {
        *c = tios.c_cc[VSTOP];
        return 0;
    }

    errno = EINVAL;
    return -1;
}

int tcflow(int fildes, int action)
{
    switch(action)
    {
        case TCOOFF:
            return ioctl(fildes, TIOCSTOP, 0);
        case TCOON:
            return ioctl(fildes, TIOCSTART, 0);
        case TCIOFF:
        case TCION:
        {
            char c;
            if (get_flow_char(fildes, action, &c) < 0) return -1;
            if (write(fildes, &c, sizeof(1)) != sizeof(c)) return -1;
            return 0;
        }
    }

    errno = EINVAL;
    return -1;
}
