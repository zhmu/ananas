/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <stdlib.h>
#include <string.h>
#include <errno.h>

extern char** environ;

int unsetenv(const char* envname)
{
    if (envname == NULL) {
        errno = EINVAL;
        return -1;
    }
    // Note that we allow 'FOO=' as an extension as it makes putenv() easier
    const char* ptr = strchr(envname, '=');
    size_t envname_len = ptr - envname;
    if (ptr == NULL)
        envname_len = strlen(envname);

    // First, find the entry
    char** e = environ;
    while (*e != NULL) {
        if (strncmp(*e, envname, envname_len) == 0 && (*e)[envname_len] == '=')
            break;
        e++;
    }

    // Keep copying the next item over ours
    for (char** next_e = e + 1; *e != NULL; /* nothing */) {
        *e++ = *next_e++;
    }

    return 0;
}
