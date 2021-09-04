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

namespace
{
    VMPage& AllocateShared(INode& inode, off_t offs, int flags)
    {
        auto vp = new VMPage(inode, offs, flags);
        vp->Lock();
        return *vp;
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

void VMPage::Map(VMArea& va, addr_t virt)
{
    AssertLocked();
    auto& vs = va.va_vs;

    int flags = va.va_flags;
    // Map COW pages as unwritable so we'll fault on a write
    if ((va.va_flags & vmarea::flag::COW) != 0 && (vp_flags & vmpage::flag::Promoted) == 0)
        flags &= ~VM_FLAG_WRITE;

    const Page* p = GetPage();
    md::vm::MapPages(vs, virt, p->GetPhysicalAddress(), 1, flags);
}

void VMPage::Zero(VMArea& va, addr_t virt)
{
    auto& vs = va.va_vs;
    KASSERT(vs.IsCurrent(), "zero outside active vmspace");

    // Clear the page XXX This is unfortunate, we should have a supply of pre-zeroed pages
    Page* p = GetPage();
    md::vm::MapPages(vs, virt, p->GetPhysicalAddress(), 1, VM_FLAG_READ | VM_FLAG_WRITE);
    memset((void*)virt, 0, PAGE_SIZE);
}

util::locked<VMPage> vmpage_lookup_locked(VMArea& va, INode& inode, off_t offs)
{
    /*
     * First step is to see if we can locate this page for the given vmspace - the private mappings
     * are stored there and override global ones.
     */
    for (auto vmpage : va.va_pages) {
        if (vmpage == nullptr || vmpage->vp_inode != &inode || vmpage->vp_offset != offs)
            continue;

        vmpage->Lock();
        return util::locked<VMPage>(*vmpage);
    }

    // Try all inode-private pages
    inode.Lock();
    for (auto vmpage : inode.i_pages) {
        // We don't check vp_inode here as this is the per-inode list already
        if (vmpage->vp_offset != offs)
            continue;

        vmpage->Lock(); // XXX is this order wise?
        inode.Unlock();
        return util::locked<VMPage>(*vmpage);
    }
    inode.Unlock();

    // Nothing was found
    return util::locked<VMPage>();
}

// Used for VMSpace::Clone (fork)
VMPage& vmpage_clone(
    VMSpace& vs_source, VMSpace& vs_dest, VMArea& va_source, VMArea& va_dest,
    util::locked<VMPage>& vp_source)
{
    KASSERT((vp_source->vp_flags & vmpage::flag::Pending) == 0, "trying to clone a pending page");

    // Always copy MD-specific pages - these tend to be modified sooner or
    // later
    if ((va_source.va_flags & VM_FLAG_MD) != 0 || true) {
        auto vp_dst = &va_dest.AllocatePrivatePage(vp_source->vp_flags);
        vp_source->Copy(*vp_dst);
        return *vp_dst;
    }

    panic("TODO clone page");
}

// Used by HandleFault()
VMPage& vmpage_clone_dentry_page(VMSpace& vs_dest, VMArea& va, util::locked<VMPage>& vmpage)
{
    KASSERT((vmpage->vp_flags & vmpage::flag::Pending) == 0, "trying to clone a pending page");

#if 0
    if ((va.va_flags & VM_FLAG_WRITE) == 0) {
        vmpage->Ref();
        return *vmpage;
    }
#endif

    auto vp_dst = &va.AllocatePrivatePage(vmpage->vp_flags | vmpage::flag::Promoted);
    vmpage->Copy(*vp_dst);
    return *vp_dst;
}

// Shared VMPages belong to an inode and not a vmspace!
util::locked<VMPage> vmpage_create_shared(INode& inode, off_t offs, int flags)
{
    VMPage& new_page = AllocateShared(inode, offs, flags);

    // Hook the vm page to the inode
    inode.Lock();
    for (auto vmpage : inode.i_pages) {
        if (vmpage->vp_offset != offs || vmpage == &new_page)
            continue;

        // Page is already present - return the one already in use
        vmpage->Lock(); // XXX is this order wise?
        inode.Unlock();

        // And throw the new page away, we won't need it
        new_page.Deref();
        return util::locked<VMPage>(*vmpage);
    }

    // Not yet present; add the new page and return it
    inode.i_pages.push_back(&new_page);
    inode.Unlock();
    return util::locked<VMPage>(new_page);
}

void VMPage::Dump(const char* prefix) const
{
    kprintf(
        "%s%p: refcount %d flags %s/%s/%c ", prefix, this, vp_refcount,
        (vp_flags & vmpage::flag::Private) ? "prv" : "pub",
        (vp_flags & vmpage::flag::ReadOnly) ? "ro" : "rw",
        (vp_flags & vmpage::flag::Pending) ? 'p' : '.');
    if (vp_page != nullptr)
        kprintf(
            " page %p phys %p order %d", vp_page, vp_page->GetPhysicalAddress(), vp_page->p_order);
    if (vp_inode != nullptr)
        kprintf(" inode %d offset %d", (int)vp_inode->i_inum, (int)vp_offset);
    kprintf("\n");
}
