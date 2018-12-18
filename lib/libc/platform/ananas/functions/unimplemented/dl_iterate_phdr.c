/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <link.h>

#pragma weak dl_iterate_phdr
int dl_iterate_phdr(int (*callback)(struct dl_phdr_info* info, size_t size, void* data), void* data)
{
    /* to be implemented by the dynamic linker - TODO: implement this for static programs */
    return 0;
}
