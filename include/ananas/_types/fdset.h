/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __FDSET_DEFINED
typedef struct {
    long fds_bits[1];
} fd_set;
#define __FDSET_DEFINED
#endif
