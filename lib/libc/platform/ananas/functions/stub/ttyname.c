/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <unistd.h>
#include <string.h>

static char s_ttyname[32];

char* ttyname(int fildes)
{
    strcpy(s_ttyname, "console");
    return s_ttyname;
}
