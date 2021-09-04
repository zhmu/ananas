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

VMPage& VMArea::AllocatePrivatePage(int flags)
{
    KASSERT((flags & vmpage::flag::Pending) == 0, "allocating pending page here?");

    auto new_page = new VMPage(flags | vmpage::flag::Private);
    new_page->Lock();

    // Hook a page to here as well, as the caller needs it anyway
    new_page->vp_page = page_alloc_single();
    KASSERT(new_page->vp_page != nullptr, "out of pages");
    return *new_page;
}

// Promote a COW page to a new writable page; returns the new page to use
VMPage& VMArea::PromotePage(VMPage& vp)
{
    kprintf("VMPage::PromotePage: vp %p\n", &vp);

    KASSERT((va_flags & vmarea::flag::COW) != 0, "attempt to promote in non-COW area");

    vp.AssertLocked();
    KASSERT((vp.vp_flags & vmpage::flag::ReadOnly) == 0, "cowing r/o page");
    KASSERT((vp.vp_flags & vmpage::flag::Promoted) == 0, "promoting promoted page %p", &vp);

    if (vp.vp_refcount == 1) {
        // Only a single reference - we can make this page writable since no one
        // else is using it
        vp.vp_flags |= vmpage::flag::Private;
        return vp;
    }

    // Page has other users - make a copy for the caller (which is no longer
    // read-only)
    auto& new_vp = AllocatePrivatePage(vp.vp_flags);
    new_vp.vp_flags |= vmpage::flag::Promoted;

    vp.Copy(new_vp);
    vp.Deref();
    return new_vp;
}
