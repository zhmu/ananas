/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <_posix/todo.h>

int getgroups(int gidsetsize, gid_t grouplist[])
{
    TODO;
    if (gidsetsize > 0)
        grouplist[0] = 0;
    return 1;
}
