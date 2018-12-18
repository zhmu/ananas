/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <errno.h>

int __cxa_thread_atexit_impl(void (*dtor_func)(void*), void* obj, void* dso_symbol)
{
    errno = ENOMEM;
    return -1;
}
