/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <EASTL/queue.h>

namespace util
{
    /*
     * Implements a queue elements of type T.
     */
    template<typename T>
    struct queue : eastl::queue<T>
    {
    };
}
