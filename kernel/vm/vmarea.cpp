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

#define DPRINTF(...)

VMArea::~VMArea()
{
    KASSERT(va_pages.empty(), "destroyed while still owning pages");
}

VMPage* VMArea::LookupVAddrAndLock(addr_t vaddr)
{
    for (auto vmpage : va_pages) {
        if (vmpage->vp_vaddr != vaddr)
            continue;

        vmpage->Lock();
        return vmpage;
    }

    // Nothing was found
    return nullptr;
}

VMPage& VMArea::AllocatePrivatePage(addr_t va, int flags)
{
    KASSERT((flags & VM_PAGE_FLAG_COW) == 0, "allocating cow page here?");
    KASSERT((flags & VM_PAGE_FLAG_PENDING) == 0, "allocating pending page here?");

    auto& new_page = vmpage::AllocatePrivate(*this, flags);

    // Hook a page to here as well, as the caller needs it anyway
    new_page.vp_vaddr = va;
    new_page.vp_page = page_alloc_single();
    KASSERT(new_page.vp_page != nullptr, "out of pages");
    return new_page;
}

VMPage& VMArea::PromotePage(VMPage& vp)
{
    KASSERT(vp.vp_vmarea == this, "wrong vmarea");

    // This promotes a COW page to a new writable page
    vp.AssertLocked();
    KASSERT((vp.vp_flags & VM_PAGE_FLAG_COW) != 0, "attempt to promote non-COW page");
    KASSERT((vp.vp_flags & VM_PAGE_FLAG_READONLY) == 0, "cowing r/o page");
    KASSERT((vp.vp_flags & VM_PAGE_FLAG_PROMOTED) == 0, "promoting promoted page %p", &vp);
    KASSERT(vp.vp_refcount > 0, "invalid refcount of vp");

    /*
     * No linked page in between - we now either:
     * (a) [refcount==1] Need to just mark the current page as non-COW, as it is the last one
     * (b) [refcount>1]  Allocate a new page, copying the current data to it
     */
    if (vp.vp_refcount == 1) {
        // (a) We *are* the source page! We can just use it
        DPRINTF(
            "vmpage_promote(): vp %p, we are the last page - using it! (page %p @ %p)\n", &vp,
            vp.vp_page, vp.vp_vaddr);
        vp.vp_flags = (vp.vp_flags & ~VM_PAGE_FLAG_COW) | VM_PAGE_FLAG_PRIVATE;
        return vp;
    }

    // (b) We have the original page - must allocate a new one, as we can't
    // touch this one (it is used by other refs we cannot touch)
    auto& new_vp = AllocatePrivatePage(vp.vp_vaddr, vp.vp_flags & ~VM_PAGE_FLAG_COW);
    DPRINTF(
        "vmpage_promote(): made new vp %p page %p for %p @ %p\n", &new_vp, new_vp.vp_page, &vp,
        new_vp.vp_vaddr);
    new_vp.vp_flags |= VM_PAGE_FLAG_PROMOTED;

    vp.Copy(new_vp);

    /*
     * Sever the connection to the vmarea; we have made a copy of the page
     * itself, so it no longer belongs there. This prevents it from being
     * shared during a clone and such.
     */
    va_pages.remove(&vp);
    vp.vp_vmarea = nullptr;
    vp.Deref();
    return new_vp;
}
