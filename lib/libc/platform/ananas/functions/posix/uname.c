/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <sys/utsname.h>
#include <string.h>

int uname(struct utsname* utsname)
{
    /* XXX */
    strcpy(utsname->sysname, "Ananas");
    strcpy(utsname->nodename, "local");
    strcpy(utsname->release, "testing");
#ifdef __amd64__
    strcpy(utsname->machine, "amd64");
#else
    /* how did you get this to compile anyway? :-) */
    strcpy(utsname->machine, "unknown");
#endif
    strcpy(utsname->version, "0.1");
    return 0;
}
