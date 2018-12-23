/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern char** environ;

int setenv(const char* envname, const char* envval, int overwrite)
{
    if (envname == NULL || strchr(envname, '=') != NULL) {
        errno = EINVAL;
        return -1;
    }
    if (overwrite && getenv(envname))
        return 0;

    int env_len = strlen(envname);
    int val_len = strlen(envval);
    if (val_len == 0)
        return unsetenv(envname);

    char* s = malloc(env_len + 1 /* = */ + val_len + 1 /* \0 */);
    if (s == NULL) {
        return -1;
    }
    sprintf(s, "%s=%s", envname, envval);
    if (putenv(s) < 0) {
        free(s);
        return -1;
    }
    return 0;
}
