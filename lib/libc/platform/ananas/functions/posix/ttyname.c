/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

static char s_ttyname[32];

char* ttyname(int fildes)
{
    if (s_ttyname[0] == '\0') {
        int result = ttyname_r(fildes, s_ttyname, sizeof(s_ttyname));
        if (result != 0) {
            errno = result;
            return NULL;
        }
    }
    return s_ttyname;
}
