/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2020 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lib.h"

// Wrappers for functions internally called by libstdc++; usually implemented
// by libsupc++
namespace std
{
    void __throw_bad_array_new_length() { panic("__throw_bad_array_new_length"); }
    void __throw_bad_alloc() { panic("__throw_bad_alloc"); }
    void __throw_length_error(const char* s) { panic("__throw_length_error: %s", s); }
}
