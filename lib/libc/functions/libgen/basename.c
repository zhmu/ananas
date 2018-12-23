/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <libgen.h>
#include <string.h>

char* basename(char* path)
{
    // Return '.' for an empty path
    if (*path == '\0')
        return ".";

    // Delete any trailing slashes
    size_t n = strlen(path) - 1;
    while (n > 0 && path[n] == '/')
        n--;
    path[n--] = '\0';

    // Isolate the last path piece
    while (n > 0 && path[n - 1] != '/')
        n--;
    return path + n;
}
