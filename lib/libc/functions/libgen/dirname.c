/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <libgen.h>
#include <string.h>

char* dirname(char* path)
{
    // Return '.' for an empty path
    if (*path == '\0')
        return ".";

    // Skip over any trailing slashes
    int n = strlen(path) - 1;
    while (n >= 0 && path[n] == '/')
        n--;

    // Locate the final slash
    while (n >= 0 && path[n] != '/')
        n--;

    if (n < 0)
        return ".";

    // Isolate the last path piece
    path[n] = '\0';
    return path;
}
