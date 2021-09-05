/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/mm.h"
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
    VMPage& AllocatePrivatePage(int flags)
    {
        KASSERT((flags & vmpage::flag::Pending) == 0, "allocating pending page here?");

        auto new_page = new VMPage(flags | vmpage::flag::Private);
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
        auto vp = new VMPage(offs, flags);
        vp->Lock();
        inode.i_pages.push_back(vp);
        inode.Unlock();
        return util::locked<VMPage>(*vp);
    }
}

VMPage::~VMPage()
{
    vp_mtx.AssertLocked();
    KASSERT(vp_refcount == 0, "freeing page with refcount %d", vp_refcount);

    // Note that we do not hold any references to the inode (the inode owns us)
    if (vp_page != nullptr)
        page_free(*vp_page);
}

void VMPage::AssertLocked()
{
    vp_mtx.AssertLocked();
    KASSERT(vp_refcount > 0, "%p invalid refcount %d", this, vp_refcount);
}

void VMPage::CopyExtended(VMPage& vp_dst, size_t len)
{
    AssertLocked();
    vp_dst.AssertLocked();
    KASSERT(this != &vp_dst, "copying same vmpage %p", this);
    KASSERT(len <= PAGE_SIZE, "copying more than a page");

    auto p_src = GetPage();
    auto p_dst = vp_dst.GetPage();
    KASSERT(p_src != p_dst, "copying same page %p", p_src);

    auto src = static_cast<char*>(kmem_map(p_src->GetPhysicalAddress(), PAGE_SIZE, VM_FLAG_READ));
    auto dst = static_cast<char*>(
        kmem_map(p_dst->GetPhysicalAddress(), PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE));

    //kprintf("VMPage CopyExtended from %p --> %p, %d bytes\n", src, dst, len);
    memcpy(dst, src, len);
    if (len < PAGE_SIZE)
        memset(dst + len, 0, PAGE_SIZE - len); // zero-fill after the data to be copied

    kmem_unmap(dst, PAGE_SIZE);
    kmem_unmap(src, PAGE_SIZE);
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

    // Map COW pages as unwritable so we'll fault on a write
    auto flags = va.va_flags;
    if ((va.va_flags & vmarea::flag::COW) != 0 && (vp_flags & vmpage::flag::Promoted) == 0)
        flags &= ~VM_FLAG_WRITE;

    const Page* p = GetPage();
    md::vm::MapPages(vs, virt, p->GetPhysicalAddress(), 1, flags);
}

void VMPage::Zero(VMSpace& vs, VMArea& va, addr_t virt)
{
    KASSERT(vs.IsCurrent(), "zero outside active vmspace");

    // Clear the page XXX This is unfortunate, we should have a supply of pre-zeroed pages
    Page* p = GetPage();
    md::vm::MapPages(vs, virt, p->GetPhysicalAddress(), 1, VM_FLAG_READ | VM_FLAG_WRITE);
    memset((void*)virt, 0, PAGE_SIZE);
}

VMPage& VMPage::Clone(VMArea& va_source, addr_t virt)
{
    KASSERT((vp_flags & vmpage::flag::Pending) == 0, "trying to clone a pending page");

    const auto IsVAFlagSet = [&](int flag) {
        return (va_source.va_flags & flag) != 0;
    };

    // Always copy MD-specific pages; don't want pagefaults in kernel stack
    if (IsVAFlagSet(VM_FLAG_MD)) {
        auto vp_dst = &vmpage::AllocatePrivatePage(vp_flags);
        Copy(*vp_dst);
        return *vp_dst;
    }

    // If the source is public or non-writable, we can re-use it
    if (!IsVAFlagSet(VM_FLAG_PRIVATE) || !IsVAFlagSet(VM_FLAG_WRITE)) {
        Ref();
        return *this;
    }

#if 0
    // Make the source COW and force the mapping to be updated
    KASSERT((vp_flags & vmpage::flag::Promoted) == 0, "cow-ing promoted page %p??", virt);
    kprintf("vmpage_clone %d: making %p cow\n", process::GetCurrent().p_pid, virt);
    va_source.va_flags |= vmarea::flag::COW;
    Map(va_source, virt);

    Ref();
    return *this;

    // XXX for now make a copy
    kprintf("TODO doing copy now for %p..%p\n", va_source.va_virt, va_source.va_virt + va_source.va_len);
#endif

    auto vp_dst = &vmpage::AllocatePrivatePage(vp_flags);
    Copy(*vp_dst);
    return *vp_dst;
}

VMPage& VMPage::Duplicate()
{
    KASSERT((vp_flags & vmpage::flag::Pending) == 0, "trying to duplicate a pending page");

    auto vp_dst = &vmpage::AllocatePrivatePage(vp_flags | vmpage::flag::Promoted);
    Copy(*vp_dst);
    return *vp_dst;
}

VMPage& VMPage::Promote()
{
    AssertLocked();
    KASSERT((vp_flags & vmpage::flag::ReadOnly) == 0, "cowing r/o page");
    KASSERT((vp_flags & vmpage::flag::Promoted) == 0, "promoting promoted page %p", this);

    if (vp_refcount == 1 && (vp_flags & vmpage::flag::Private) == 0) {
        // Only a single reference - we can make this page writable since no one
        // else is using it
        kprintf("Promote %d/%p: making it writable\n", process::GetCurrent().p_pid, this);
        vp_flags |= vmpage::flag::Private | vmpage::flag::Promoted;
        return *this;
    }

    // Page has other users - make a copy for the caller (which is no longer
    // read-only)
    kprintf("Promote %d/%p: making a copy\n", process::GetCurrent().p_pid, this);
    return Duplicate();
}

void VMPage::Dump(const char* prefix) const
{
    kprintf(
        "%s%p: refcount %d flags %s/%s/%c offset %d", prefix, this, vp_refcount,
        (vp_flags & vmpage::flag::Private) ? "prv" : "pub",
        (vp_flags & vmpage::flag::ReadOnly) ? "ro" : "rw",
        (vp_flags & vmpage::flag::Pending) ? 'p' : '.',
        static_cast<int>(vp_offset));
    if (vp_page != nullptr)
        kprintf(
            " page %p phys %p order %d", vp_page, vp_page->GetPhysicalAddress(), vp_page->p_order);
    kprintf("\n");
}
