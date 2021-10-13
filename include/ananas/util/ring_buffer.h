/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <EASTL/bonus/fixed_ring_buffer.h>

namespace util
{
    /*
     * Implements a ring buffer of fixed size N containing elements of type T.
     */
    template<typename T, size_t N>
    struct ring_buffer final : eastl::fixed_ring_buffer<T, N>
    {
        ring_buffer() : eastl::fixed_ring_buffer<T, N>(N) { }
    };
}
