/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef _MAP_STATUSCODE_H_
#define _MAP_STATUSCODE_H_

#include <ananas/statuscode.h>
#include <errno.h>

static inline int map_statuscode(statuscode_t status)
{
    if (ananas_statuscode_is_failure(status)) {
        errno = ananas_statuscode_extract_errno(status);
        return -1;
    }

    return ananas_statuscode_extract_value(status);
}

#endif /* _MAP_STATUSCODE_H_ */
