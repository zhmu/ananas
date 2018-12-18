/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <dlfcn.h>

#pragma weak dladdr

int dladdr(const void* addr, Dl_info* info) { return 0; }
