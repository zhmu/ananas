/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <string.h>
#include <ananas/tty.h>
#include <sys/ioctl.h>
#include <errno.h>

const char tty_dev_prefix[] = "/dev/";

int ttyname_r(int fildes, char* name, size_t namesize)
{
    size_t tty_dev_prefix_len = strlen(tty_dev_prefix);
    if (namesize <= tty_dev_prefix_len) {
        return ERANGE;
    }
    memcpy(name, tty_dev_prefix, tty_dev_prefix_len);

    if (ioctl(fildes, TIOGDNAME, name + tty_dev_prefix_len, namesize - tty_dev_prefix_len - 1) < 0) {
        return errno;
    }
    return 0;
}
