/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/syscalls.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* XXX This is just a copy of execl() for now ... */
int execlp(const char* path, const char* arg0, ...)
{
    va_list va;

    /* Count the number of arguments */
    int num_args = 1 /* terminating \0 */;
    va_start(va, arg0);
    while (1) {
        const char* arg = va_arg(va, const char*);
        if (arg == NULL)
            break;
        num_args++;
    }
    va_end(va);

    /* Construct the argument list */
    const char** args = malloc(num_args * sizeof(char*));
    if (args == NULL) {
        errno = ENOMEM;
        return -1;
    }
    int pos = 0;
    va_start(va, arg0);
    while (1) {
        const char* arg = va_arg(va, const char*);
        if (arg == NULL)
            break;
        args[pos++] = arg;
    }
    args[pos] = NULL;
    va_end(va);

    execve(path, (char* const*)args, environ);

    /* If we got here, execve() failed (and set errno) and we must clean up after ourselves */
    free(args);
    return -1;
}

/* vim:set ts=2 sw=2: */
