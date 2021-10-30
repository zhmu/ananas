/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"

struct VMPage;
struct VMSpace;

namespace userland
{
    VMPage& CreateStack(VMSpace& vs, const addr_t stackEnd);
    VMPage& CreateCode(VMSpace& vs, const addr_t virt, const addr_t len);

    void* MapPageToKernel(VMPage& vp);
    void UnmapPage(void* p);

    size_t CalculateVectorStorageInBytes(const char* p[], size_t& num_entries);
    size_t CalculatePaddingBytesNeeded(size_t n);

    template<typename T>
    void Store(addr_t& ptr, const T& data, size_t n)
    {
        memcpy(reinterpret_cast<void*>(ptr), &data, n);
        ptr += n;
    }

    template<typename T>
    void Store(addr_t& ptr, const T& data)
    {
        Store(ptr, data, sizeof(data));
    }
}
