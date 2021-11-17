/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <netdb.h>

struct hostent* gethostbyname(const char* name)
{
    // TODO
    h_errno = HOST_NOT_FOUND;
    return NULL;
}
