/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vmpage.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"
#include "kernel-md/md.h"
#include "kernel-md/vm.h"

#include "kernel/process.h"

namespace vmpage
{
    VMPage& Allocate(int flags)
    {
        KASSERT((flags & vmpage::flag::Pending) == 0, "allocating pending page here?");

        auto new_page = new VMPage(flags, -1);
        new_page->Lock();

        // Hook a page to here as well, as the caller needs it anyway
        new_page->vp_page = page_alloc_single();
        KASSERT(new_page->vp_page != nullptr, "out of pages");
        return *new_page;
    }

    util::locked<VMPage> LookupOrCreateINodePage(INode& inode, off_t offs, int flags)
    {
        inode.Lock();
        for (auto vp: inode.i_pages) {
            if (vp->vp_offset != offs)
                continue;

            // Page is already present; return it
            vp->Lock();
            inode.Unlock();
            return util::locked<VMPage>(*vp);
        }

        // Not yet present; create a new page and return it
        auto vp = new VMPage(flags, offs);
        vp->Lock();
        inode.i_pages.push_back(vp);
        inode.Unlock();
        return util::locked<VMPage>(*vp);
    }
}

namespace
{
    /*
     * Copies a (piece of) vp_src to vp_dst:
     *
     *        len
     *         /
     *        +-------+------+
     * vp_src |XXXXXXX|??????|
     *        +-------+------+
     *            |
     *            v
     *        +-------+---+
     * vp_dst |XXXXXXX|000|
     *        +-------+---+
     *                ^    \
     *               len    PAGE_SIZE
     *
     *
     * X = content to be copied, 0 = bytes set to zero, ? = don't care - note that
     * thus the vp_dst page is always completely filled.
     */
    void Copy(VMPage& vp_src, VMPage& vp_dst, const size_t len)
    {
        vp_src.AssertLocked();
        vp_dst.AssertLocked();
        KASSERT(&vp_src != &vp_dst, "copying same vmpage %p", &vp_src);
        KASSERT(len <= PAGE_SIZE, "copying more than a page");

        auto p_src = vp_src.GetPage();
        auto p_dst = vp_dst.GetPage();
        KASSERT(p_src != p_dst, "copying same page %p", p_src);

        auto src = static_cast<char*>(kmem_map(p_src->GetPhysicalAddress(), PAGE_SIZE, vm::flag::Read));
        auto dst = static_cast<char*>(
            kmem_map(p_dst->GetPhysicalAddress(), PAGE_SIZE, vm::flag::Read | vm::flag::Write));

        memcpy(dst, src, len);
        if (len < PAGE_SIZE)
            memset(dst + len, 0, PAGE_SIZE - len); // zero-fill after the data to be copied

        kmem_unmap(dst, PAGE_SIZE);
        kmem_unmap(src, PAGE_SIZE);
    }
}


VMPage::~VMPage()
{
    vp_mtx.AssertLocked();
    KASSERT(vp_refcount == 0, "freeing page with refcount %d", vp_refcount);

    // Note that we do not hold any references to the inode (the inode owns us)
    if (vp_page != nullptr) {
        page_free(*vp_page);
        vp_page = nullptr;
    }

    Unlock();
}

void VMPage::AssertLocked()
{
    vp_mtx.AssertLocked();
    KASSERT(vp_refcount > 0, "%p invalid refcount %d", this, vp_refcount);
}

void VMPage::Ref()
{
    AssertLocked();
    ++vp_refcount;
}

void VMPage::Deref()
{
    AssertLocked();
    KASSERT(vp_refcount > 0, "invalid refcount %d", vp_refcount);
    if (--vp_refcount > 0) {
        Unlock();
        return;
    }
    delete this;
}

void VMPage::Map(VMSpace& vs, VMArea& va, addr_t virt)
{
    AssertLocked();

    auto flags = va.va_flags;
    if ((vp_flags & vmpage::flag::Promoted) == 0)
        flags &= ~vm::flag::Write;

    if ((vp_flags & vmpage::flag::Inode) != 0) {
        KASSERT((flags & vm::flag::Write) == 0, "writing to an inode page??");
    }

    const Page* p = GetPage();
    md::vm::MapPages(vs, virt, p->GetPhysicalAddress(), 1, flags);
}

void VMPage::Zero(VMSpace& vs, VMArea& va, addr_t virt)
{
    KASSERT(vs.IsCurrent(), "zero outside active vmspace");

    // Clear the page XXX This is unfortunate, we should have a supply of pre-zeroed pages
    Page* p = GetPage();
    md::vm::MapPages(vs, virt, p->GetPhysicalAddress(), 1, vm::flag::Read | vm::flag::Write);
    memset(reinterpret_cast<void*>(virt), 0, PAGE_SIZE);
}

VMPage& VMPage::Duplicate(size_t copy_length)
{
    AssertLocked();
    KASSERT((vp_flags & vmpage::flag::Pending) == 0, "trying to duplicate a pending page");

    auto vp_dst = &vmpage::Allocate(vp_flags | vmpage::flag::Promoted);
    vp_dst->vp_flags &= ~vmpage::flag::Inode;
    Copy(*this, *vp_dst, copy_length);
    return *vp_dst;
}

VMPage& VMPage::Promote()
{
    AssertLocked();
    KASSERT((vp_flags & vmpage::flag::Promoted) == 0, "promoting promoted page %p", this);
    KASSERT((vp_flags & vmpage::flag::Inode) == 0, "promoting inode page %p", this);

    if (vp_refcount == 1) {
        // Only a single reference - we can make this page writable since no one
        // else is using it
        vp_flags |= vmpage::flag::Promoted;
        return *this;
    }

    // Page has other users - make a writable copy for the caller
    return Duplicate(PAGE_SIZE);
}

void VMPage::Dump(const char* prefix) const
{
    kprintf(
        "%s%p: refcount %d flags %c offset %d", prefix, this, vp_refcount,
        (vp_flags & vmpage::flag::Pending) ? 'p' : '.',
        static_cast<int>(vp_offset));
    if (vp_page != nullptr)
        kprintf(
            " page %p phys %p order %d", vp_page, vp_page->GetPhysicalAddress(), vp_page->p_order);
    kprintf("\n");
}
