/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/util/utility.h>
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel/vmpage.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vm.h"

namespace
{
    Result read_data(DEntry& dentry, void* buf, off_t offset, size_t len)
    {
        struct VFS_FILE f;
        memset(&f, 0, sizeof(f));
        f.f_dentry = &dentry;

        if (auto result = vfs_seek(&f, offset); result.IsFailure())
            return result;

        auto result = vfs_read(&f, buf, len);
        if (result.IsFailure())
            return result;

        auto numread = result.AsValue();
        if (numread != len)
            return Result::Failure(EIO);
        return Result::Success();
    }

    int vmspace_page_flags_from_va(const VMArea& va)
    {
        int flags = 0;
        if ((va.va_flags & (VM_FLAG_READ | VM_FLAG_WRITE)) == VM_FLAG_READ) {
            flags |= vmpage::flag::ReadOnly;
        }
        return flags;
    }

    void AssignPageToVirtualAddress(VMArea& va, const VAInterval& interval, const addr_t virt, VMPage& vmpage)
    {
        const auto page_index = (virt - interval.begin) / PAGE_SIZE;
        auto& vp = va.va_pages[page_index];
        if (vp) vp->Deref();
        vp = &vmpage;
        vmpage.Map(va, virt);
    }

    util::locked<VMPage> vmspace_get_dentry_backed_page(VMArea& va, off_t read_off)
    {
        // First, try to lookup the page; if we already have it, no need to read it
        util::locked<VMPage> vmpage = vmpage_lookup_locked(va, *va.va_dentry->d_inode, read_off);
        if (!vmpage) {
            // Page not found - we need to allocate one. This is always a shared mapping, which
            // we'll copy if needed
            vmpage = vmpage_create_shared(
                *va.va_dentry->d_inode, read_off,
                vmpage::flag::Pending | vmspace_page_flags_from_va(va));
        }
        // vmpage will be locked at this point!

        if ((vmpage->vp_flags & vmpage::flag::Pending) == 0)
            return vmpage;

        // Read the page - note that we hold the vmpage lock while doing this
        Page* p;
        void* page = page_alloc_single_mapped(p, VM_FLAG_READ | VM_FLAG_WRITE);
        KASSERT(p != nullptr, "out of memory"); // XXX handle this

        size_t read_length = PAGE_SIZE;
        if (read_off + read_length > va.va_dentry->d_inode->i_sb.st_size) {
            // This inode is simply not long enough to cover our read - adjust XXX what when it
            // grows?
            read_length = va.va_dentry->d_inode->i_sb.st_size - read_off;
            // Zero out everything after the part we will read so we don't leak any data
            memset(static_cast<char*>(page) + read_length, 0, PAGE_SIZE - read_length);
        }

        Result result = read_data(*va.va_dentry, page, read_off, read_length);
        kmem_unmap(page, PAGE_SIZE);
        KASSERT(result.IsSuccess(), "cannot deal with error %d", result.AsStatusCode()); // XXX

        // Update the vm page to contain our new address
        vmpage->vp_page = p;
        vmpage->vp_flags &= ~vmpage::flag::Pending;
        return vmpage;
    }
} // unnamed namespace


void DumpVMSpace(VMSpace& vmspace)
{
    auto prev = 0;
    for (auto& [ interval, va ] : vmspace.vs_areamap) {
        kprintf(
            "[%p..%p) (%p..%p) %c%c%c\n",
            interval.begin,
            interval.end,
            reinterpret_cast<void*>(va->va_virt),
            reinterpret_cast<void*>(va->va_virt + va->va_len - 1),
            (va->va_flags & VM_FLAG_READ) ? 'r' : '-',
            (va->va_flags & VM_FLAG_WRITE) ? 'w' : '-',
            (va->va_flags & VM_FLAG_EXECUTE) ? 'x' : '-');
        if (prev == interval.begin) {
            panic("DUP");
        }
        prev = interval.begin;
    }
    kprintf("dump end\n");
}

Result VMSpace::HandleFault(addr_t virt, const int fault_flags)
{
    //kprintf(">> HandleFault(): vs=%p, virt=%p, flags=0x%x\n", this, virt, fault_flags);

    // Walk through the areas one by one
    for (auto& [interval, va ] : vs_areamap) {
        if (!(virt >= va->va_virt && (virt < (va->va_virt + va->va_len))))
            continue;

        /* We should only get faults for lazy areas (filled by a function) or when we have to
         * dynamically allocate things */
        KASSERT(
            (va->va_flags & VM_FLAG_FAULT) != 0,
            "unexpected pagefault in area %p, virt=%p, len=%d, flags 0x%x", &va, va->va_virt,
            va->va_len, va->va_flags);

        // See if we have this page mapped
        const auto alignedVirt = virt & ~(PAGE_SIZE - 1);
        VMPage* vp = va->LookupVAddrAndLock(alignedVirt);
        if (vp != nullptr) {
            if ((fault_flags & VM_FLAG_WRITE) && (va->va_flags & vmarea::flag::COW)) {
                // Promote our copy to a writable page and update the mapping
                KASSERT((vp->vp_flags & vmpage::flag::ReadOnly) == 0, "cowing r/o page");
                auto& new_vp = vp->Promote();
                if (&new_vp != vp) vp->Unlock();
                AssignPageToVirtualAddress(*va, interval, alignedVirt, new_vp);
                new_vp.Unlock();
                return Result::Success();
            }

            // Page is already mapped, but not COW. Bad, reject
            vp->Unlock();
            return Result::Failure(EFAULT);
        }

        // XXX we expect va_doffset to be page-aligned here (i.e. we can always use a page directly)
        // this needs to be enforced when making mappings!
        KASSERT(
            (va->va_doffset & (PAGE_SIZE - 1)) == 0, "doffset %x not page-aligned",
            (int)va->va_doffset);

        // If there is a dentry attached here, perhaps we may find what we need in the corresponding
        // inode
        if (va->va_dentry != nullptr) {
            /*
             * The way dentries are mapped to virtual address is:
             *
             * 0       va_doffset                               file length
             * +------------+-------------+-------------------------------+
             * |            |XXXXXXXXXXXXX|                               |
             * |            |XXXXXXXXXXXXX|                               |
             * +------------+-------------+-------------------------------+
             *             /     |||      \ va_doffset + va_dlength
             *            /      vvv
             *     +-------------+---------------+
             *     |XXXXXXXXXXXXX|000000000000000|
             *     |XXXXXXXXXXXXX|000000000000000|
             *     +-------------+---------------+
             *     0            \
             *                   \
             *                    va_dlength
             */
            const auto read_off = alignedVirt - va->va_virt; // offset in area, still needs va_doffset added
            if (read_off < va->va_dlength) {
                // At least (part of) the page is to be read from the backing dentry -
                // this means we want the entire page
                util::locked<VMPage> vmpage =
                    vmspace_get_dentry_backed_page(*va, read_off + va->va_doffset);

                // If the mapping is page-aligned and read-only or shared, we can re-use the
                // mapping and avoid the entire copy
                VMPage* new_vp;
                bool can_reuse_page_1on1 = true;
                // Reusing means the page resides in the section...
                can_reuse_page_1on1 &= (read_off + PAGE_SIZE) <= va->va_dlength;
                // ... and we have a page-aligned offset
                can_reuse_page_1on1 &= (va->va_doffset & (PAGE_SIZE - 1)) == 0;
                if (can_reuse_page_1on1) {
                    // Just clone the page; it could both be an inode-backed page (if
                    // this is a private COW) or vmspace-backed if we are COW-ing from a
                    // parent
                    new_vp = &vmpage_clone_dentry_page(*this, *va, vmpage);
                } else {
                    // Cannot re-use; create a new VM page, with appropriate flags based on the va
                    new_vp = &vmpage::AllocatePrivatePage(
                        vmpage::flag::Private | vmspace_page_flags_from_va(*va));

                    // Now copy the parts of the dentry-backed page
                    size_t copy_len =
                        va->va_dlength - read_off; // this is size-left after where we read
                    if (copy_len > PAGE_SIZE)
                        copy_len = PAGE_SIZE;
                    vmpage->CopyExtended(*new_vp, copy_len);
                }
                if (new_vp != &*vmpage)
                    vmpage.Unlock();

                AssignPageToVirtualAddress(*va, interval, alignedVirt, *new_vp);
                new_vp->Unlock();
                return Result::Success();
            }
        }

        // We need a new VM page here; this is an anonymous mapping which we need to back
        //kprintf("VMFault: new zeroed page for %p\n", alignedVirt);
        auto& new_vp = vmpage::AllocatePrivatePage(vmpage::flag::Private);
        new_vp.Zero(*va, alignedVirt);
        AssignPageToVirtualAddress(*va, interval, alignedVirt, new_vp);
        new_vp.Unlock();
        return Result::Success();
    }

    return Result::Failure(EFAULT);
}
