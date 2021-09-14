/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/socket.h>

#ifndef __SYS_UN_H__
#define __SYS_UN_H__

#define UNIX_PATH_MAX 96

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

#endif /* __SYS_UN_H__ */
