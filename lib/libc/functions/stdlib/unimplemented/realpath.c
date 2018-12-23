/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <stdlib.h>
#include <string.h>

char* realpath(const char* file_name, char* _PDCLIB_restrict resolved_name)
{
    strcpy(resolved_name, file_name);
    return resolved_name;
}
