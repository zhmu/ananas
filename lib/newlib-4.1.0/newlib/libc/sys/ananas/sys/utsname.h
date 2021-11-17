/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYS_UTSNAME_H__
#define __SYS_UTSNAME_H__

#include <sys/cdefs.h>

#define UTSNAME_SYSNAME_LEN 64
#define UTSNAME_NODENAME_LEN 16
#define UTSNAME_RELEASE_LEN 16
#define UTSNAME_MACHINE_LEN 16
#define UTSNAME_VERSION_LEN 16

struct utsname {
    char sysname[UTSNAME_SYSNAME_LEN];
    char nodename[UTSNAME_NODENAME_LEN];
    char release[UTSNAME_RELEASE_LEN];
    char machine[UTSNAME_MACHINE_LEN];
    /* fields below here are extensions */
    char version[UTSNAME_VERSION_LEN];
};

__BEGIN_DECLS

int uname(struct utsname* utsname);

__END_DECLS

#endif /* __SYS_UTSNAME_H__ */
