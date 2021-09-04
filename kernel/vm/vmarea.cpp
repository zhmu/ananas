/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/vmpage.h"
#include "kernel/vmarea.h"
#include "kernel/page.h"
#include "kernel/lib.h"

#include "kernel/vmspace.h"

VMArea::VMArea(VMSpace& vs, addr_t virt, size_t len, int flags)
    : va_vs(vs), va_virt(virt), va_len(len), va_flags(flags)
{
    const auto numberOfPages = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    va_pages.resize(numberOfPages);
}

VMArea::~VMArea()
{
    size_t n{};
    for(auto vp: va_pages) {
        if (vp != nullptr) ++n;
    }
    KASSERT(n == 0, "vmarea destroyed while still holding %d page(s)", n);
}

VMPage* VMArea::LookupVAddrAndLock(addr_t vaddr)
{
    if (vaddr < va_virt) return nullptr;
    if (vaddr >= va_virt + va_len) return nullptr;

    auto& vmpage = va_pages[(vaddr - va_virt) / PAGE_SIZE];
    if (vmpage == nullptr) return nullptr;

    vmpage->Lock();
    return vmpage;
}
