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

#define DPRINTF(...)

namespace vmpage
{
    VMPage& Allocate(VMArea& va, INode* inode, off_t offset, int flags)
    {
        auto vp = new VMPage(&va, inode, offset, flags);

        vp->Lock();
        KASSERT((vp->vp_flags & VM_PAGE_FLAG_PRIVATE) != 0, "dafuq");
        va.va_pages.push_back(*vp);
        return *vp;
    }

} // namespace vmpage

VMPage::~VMPage()
{
    vp_mtx.AssertLocked();
    KASSERT(vp_refcount == 0, "freeing page with refcount %d", vp_refcount);

    // If we are hooked to a vmarea, unlink us
    if (vp_vmarea != nullptr)
        vp_vmarea->va_pages.remove(*this);

    // Note that we do not hold any references to the inode (the inode owns us)
    if (vp_flags & VM_PAGE_FLAG_LINK) {
        if (vp_link != nullptr) {
            vp_link->Lock();
            vp_link->Deref();
        }
    } else {
        if (vp_page != nullptr)
            page_free(*vp_page);
    }
}

void VMPage::AssertLocked()
{
    vp_mtx.AssertLocked();
    KASSERT(vp_refcount > 0, "%p invalid refcount %d", this, vp_refcount);
    KASSERT(
        ((vp_flags & VM_PAGE_FLAG_LINK) == 0) || ((vp_link->vp_flags & VM_PAGE_FLAG_LINK) == 0),
        "%p links to linked page %p", this, vp_link);
}

VMPage& VMPage::Link(VMArea& va)
{
    AssertLocked();
    VMPage& vp_source = [](VMPage& vp) -> VMPage& {
        auto& vp_resolved = vp.Resolve();
        if (&vp_resolved == &vp)
            return vp;
        vp_resolved.Lock();
        return vp_resolved;
    }(*this);
    // this and vp_source are both locked now

    // Increase source refcount as we will be linked towards it
    vp_source.Ref();

    int flags = VM_PAGE_FLAG_PRIVATE | VM_PAGE_FLAG_LINK;
    if (vp_source.vp_flags & VM_PAGE_FLAG_READONLY)
        flags |= VM_PAGE_FLAG_READONLY;

    auto& vp_new = vmpage::Allocate(va, vp_source.vp_inode, vp_source.vp_offset, flags);
    vp_new.vp_link = &vp_source;
    vp_new.vp_vaddr = vp_source.vp_vaddr;

    if (&vp_source != this)
        vp_source.Unlock();
    return vp_new;
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

void VMPage::Map(VMSpace& vs, VMArea& va)
{
    KASSERT(vp_vmarea == &va, "bad va");
    KASSERT(&vp_vmarea->va_vs == &vs, "bad vs");
    AssertLocked();

    int flags = va.va_flags;
    // Map COW pages as unwritable so we'll fault on a write
    if (vp_flags & VM_PAGE_FLAG_COW)
        flags &= ~VM_FLAG_WRITE;
    const Page* p = GetPage();

    md::vm::MapPages(&vs, vp_vaddr, p->GetPhysicalAddress(), 1, flags);
}

void VMPage::Zero(VMSpace& vs)
{
    KASSERT(&vp_vmarea->va_vs == &vs, "bad vs");

    // Clear the page XXX This is unfortunate, we should have a supply of pre-zeroed pages
    Page* p = GetPage();
    md::vm::MapPages(&vs, vp_vaddr, p->GetPhysicalAddress(), 1, VM_FLAG_READ | VM_FLAG_WRITE);
    // XXX We should check if vs is the current vmspace; otherwise the memset will fail
    memset((void*)vp_vaddr, 0, PAGE_SIZE);
}

util::locked<VMPage> vmpage_lookup_locked(VMArea& va, INode& inode, off_t offs)
{
    /*
     * First step is to see if we can locate this page for the given vmspace - the private mappings
     * are stored there and override global ones.
     */
    for (auto& vmpage : va.va_pages) {
        if (vmpage.vp_inode != &inode || vmpage.vp_offset != offs)
            continue;

        vmpage.Lock();
        return util::locked<VMPage>(vmpage);
    }

    // Try all inode-private pages
    inode.Lock();
    for (auto& vmpage : inode.i_pages) {
        // We don't check vp_inode here as this is the per-inode list already
        if (vmpage.vp_offset != offs)
            continue;

        vmpage.Lock(); // XXX is this order wise?
        inode.Unlock();
        return util::locked<VMPage>(vmpage);
    }
    inode.Unlock();

    // Nothing was found
    return util::locked<VMPage>();
}

VMPage& vmpage_clone(
    VMSpace* vs_source, VMSpace& vs_dest, VMArea& va_source, VMArea& va_dest,
    util::locked<VMPage>& vp_orig)
{
    KASSERT(vs_source == nullptr || &va_source == vp_orig->vp_vmarea, "area mismatch");

    /*
     * Cloning a page may results in different scenarios:
     *
     * 1) We can create a link to the source page, using the same access
     * 2) We can create a link to the source page, yet employ COW
     *    This consists of marking the original page as read-only.
     * 3) We need to duplicate the source page
     */
    auto& vp_source = [](util::locked<VMPage>& vp) -> VMPage& {
        auto& vp_resolved = vp->Resolve();
        if (&vp_resolved == &*vp)
            return *vp;
        vp_resolved.Lock();
        return vp_resolved;
    }(vp_orig);
    // both vp_source and vp_orig will be locked here
    KASSERT((vp_source.vp_flags & VM_PAGE_FLAG_PENDING) == 0, "trying to clone a pending page");
    KASSERT((vp_source.vp_flags & VM_PAGE_FLAG_LINK) == 0, "trying to clone a link page");

    // (3) If this is a MD-specific page, just duplicate it - we don't want to share things like
    // pagetables and the like
    VMPage* vp_dst;
    if (va_source.va_flags & VM_FLAG_MD) {
        vp_dst = &va_dest.AllocatePrivatePage(vp_source.vp_vaddr, vp_source.vp_flags);
        vp_source.Copy(*vp_dst);
    } else if (vp_source.vp_flags & VM_PAGE_FLAG_READONLY) {
        // (1) If the source is read-only, we can always share it
        vp_dst = &vp_source.Link(va_dest);
    } else {
        // (2) Clone the page using COW; note that vp_orig may be a linked page
        KASSERT(vs_source == nullptr || vs_source != &vs_dest, "cloning to same vmspace");
        KASSERT(vs_source == nullptr || &va_source != &va_dest, "cloning to same vmarea");
        vp_source.AssertLocked();
        KASSERT((vp_source.vp_flags & VM_PAGE_FLAG_LINK) == 0, "cow'ing linked page");

        // Mark the source page as COW and update the mapping - this makes it
        // read-only. Note tha we must do this for the *original* page, not a
        // possibly linked page
        vp_orig->vp_flags |= VM_PAGE_FLAG_COW;
        vp_orig->vp_flags &= ~VM_PAGE_FLAG_PROMOTED;
        KASSERT((vp_orig->vp_flags & VM_PAGE_FLAG_READONLY) == 0, "cowing r/o page");
        if (vs_source != nullptr)
            vp_orig->Map(*vs_source, va_source);

        if (true) {
            // Return a link to the page, but do mark it as COW as well
            vp_dst = &vp_source.Link(va_dest);
            vp_dst->vp_flags |= VM_PAGE_FLAG_COW;
            KASSERT((vp_dst->vp_flags & VM_PAGE_FLAG_READONLY) == 0, "cowing r/o page");
            vp_dst->vp_vaddr = vp_source.vp_vaddr;
        } else {
            // This can be used to bypass COW and force a copy
            vp_dst = &va_dest.AllocatePrivatePage(vp_source.vp_vaddr, vp_source.vp_flags);
            vp_source.Copy(*vp_dst);
        }
    }

    // Unlock the original page
    if (&*vp_orig != &vp_source)
        vp_source.Unlock();
    return *vp_dst;
}

Page* VMPage::GetPage()
{
    VMPage* vp = this;
    if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
        vp = vp->vp_link;
    }

    return vp->vp_page;
}

VMPage& VMPage::Resolve()
{
    VMPage* vp = this;
    KASSERT(vp->vp_refcount > 0, "invalid refcount %d", vp_refcount);
    if (vp->vp_flags & VM_PAGE_FLAG_LINK) {
        vp = vp->vp_link;
        KASSERT(vp->vp_refcount > 0, "invalid refcount for %p (%d)", vp, vp_refcount);
        KASSERT((vp->vp_flags & VM_PAGE_FLAG_LINK) == 0, "%p links to a linked page %p", this, vp);
    }
    return *vp;
}

void VMPage::AssertNotLinked()
{
    KASSERT((vp_flags & VM_PAGE_FLAG_LINK) == 0, "vmpage %p is linked", this);
}

util::locked<VMPage> vmpage_create_shared(INode& inode, off_t offs, int flags)
{
    VMPage& new_page = *new VMPage(nullptr, &inode, offs, flags);
    new_page.Lock();

    // Hook the vm page to the inode
    inode.Lock();
    for (auto& vmpage : inode.i_pages) {
        if (vmpage.vp_offset != offs || &vmpage == &new_page)
            continue;

        // Page is already present - return the one already in use
        vmpage.Lock(); // XXX is this order wise?
        inode.Unlock();

        // And throw the new page away, we won't need it
        new_page.Deref();
        return util::locked<VMPage>(vmpage);
    }

    // Not yet present; add the new page and return it
    inode.i_pages.push_back(new_page);
    inode.Unlock();
    return util::locked<VMPage>(new_page);
}

void VMPage::Dump(const char* prefix) const
{
    kprintf(
        "%s%p: refcount %d vaddr %p flags %s/%s/%s/%c ", prefix, this, vp_refcount, vp_vaddr,
        (vp_flags & VM_PAGE_FLAG_PRIVATE) ? "prv" : "pub",
        (vp_flags & VM_PAGE_FLAG_READONLY) ? "ro" : "rw",
        (vp_flags & VM_PAGE_FLAG_COW) ? "cow" : "---",
        (vp_flags & VM_PAGE_FLAG_PENDING) ? 'p' : '.');
    if (vp_flags & VM_PAGE_FLAG_LINK) {
        kprintf(" -> ");
        vp_link->Dump("");
        return;
    }
    if (vp_page != nullptr)
        kprintf(
            " page %p phys %p order %d", vp_page, vp_page->GetPhysicalAddress(), vp_page->p_order);
    if (vp_inode != nullptr)
        kprintf(" inode %d offset %d", (int)vp_inode->i_inum, (int)vp_offset);
    kprintf("\n");
}
