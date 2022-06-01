/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
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
    Result PerformDEntryRead(DEntry& dentry, void* buf, off_t offset, size_t len)
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

    void AssignPageToVirtualAddress(VMSpace& vs, VMArea& va, const VAInterval& interval, const addr_t virt, VMPage& vmpage)
    {
        const auto page_index = (virt - interval.begin) / PAGE_SIZE;
        auto& vp = va.va_pages[page_index];

        auto previousVP = vp;
        vp = &vmpage;
        vmpage.Map(vs, va, virt);

        if (previousVP && previousVP != &vmpage) {
            previousVP->AssertLocked();
            previousVP->Deref();
        }
    }

    // The page returned here will be owned by the inode and is shared
    util::locked<VMPage> GetDEntryBackedPage(VMArea& va, const off_t read_off)
    {
        auto vmpage = vmpage::LookupOrCreateINodePage(
                *va.va_dentry->d_inode, read_off,
                vmpage::flag::Inode | vmpage::flag::Pending);
        if ((vmpage->vp_flags & vmpage::flag::Pending) == 0)
            return vmpage;

        // Read the page - note that we hold the vmpage lock while doing this
        Page* p;
        void* page = page_alloc_single_mapped(p, vm::flag::Read | vm::flag::Write);
        KASSERT(p != nullptr, "out of memory"); // XXX handle this

        size_t read_length = PAGE_SIZE;
        if (read_off + read_length > va.va_dentry->d_inode->i_sb.st_size) {
            // Do not read beyond what is available; the remainder must be
            // zero-filled
            read_length = va.va_dentry->d_inode->i_sb.st_size - read_off;
            memset(static_cast<char*>(page) + read_length, 0, PAGE_SIZE - read_length);
        }

        const auto result = PerformDEntryRead(*va.va_dentry, page, read_off, read_length);
        kmem_unmap(page, PAGE_SIZE);
        KASSERT(result.IsSuccess(), "cannot deal with error %d", result.AsStatusCode()); // XXX

        // Update the vm page to contain our new address
        vmpage->vp_page = p;
        vmpage->vp_flags &= ~vmpage::flag::Pending;
        return vmpage;
    }

    VMPage* HandleDEntryBackedFault(VMSpace& vs, VMArea& va, const VAInterval& interval, const bool isWriteFault, const addr_t alignedVirt)
    {
        const auto read_off = alignedVirt - va.va_virt; // offset in area, still needs va_doffset added
        if (read_off >= va.va_dlength)
            return nullptr; // outside of dentry; must be zero-filled

        // At least (part of) the page is to be read from the backing dentry -
        // this means we want the entire page
        auto vmpage = GetDEntryBackedPage(va, read_off + va.va_doffset);

        const auto is_writable = (va.va_flags & vm::flag::Write) != 0;
        const auto can_be_shared =
            // Mapping must completely be backed by the inode (i.e. not zero-padded)
            ((read_off + PAGE_SIZE) <= va.va_dlength)
            // And we must not be able to write to it
            && !is_writable;
        if (can_be_shared) {
            vmpage->Ref();
            return vmpage.Extract();
        }

        // Make a copy
        size_t copy_len =
            va.va_dlength - read_off; // this is size-left after where we read
        if (copy_len > PAGE_SIZE)
            copy_len = PAGE_SIZE;
        auto new_vp = &vmpage->Duplicate(copy_len);
        vmpage.Unlock();
        return new_vp;
    }
} // unnamed namespace

void DumpVMSpace(VMSpace& vmspace)
{
    for (auto& [ interval, va ] : vmspace.vs_areamap) {
        kprintf(
            "[%p..%p) (%p..%p) %c%c%c\n",
            interval.begin,
            interval.end,
            reinterpret_cast<void*>(va->va_virt),
            reinterpret_cast<void*>(va->va_virt + va->va_len - 1),
            (va->va_flags & vm::flag::Read) ? 'r' : '-',
            (va->va_flags & vm::flag::Write) ? 'w' : '-',
            (va->va_flags & vm::flag::Execute) ? 'x' : '-');
    }
    kprintf("dump end\n");
}

Result VMSpace::HandleFault(const addr_t virt, const int fault_flags)
{
    const auto isWriteFault = (fault_flags & vm::flag::Write) != 0;
    //kprintf(">> HandleFault(): vs=%p, virt=%p, flags=0x%x\n", this, virt, fault_flags);

    // Look up the fault address in the area map
    auto it = vs_areamap.find_by_value(virt);
    if (it == vs_areamap.end()) return Result::Failure(EFAULT);

    auto& interval = it->interval;
    auto& va = it->value;

    // See if we have this page mapped
    const auto alignedVirt = virt & ~(PAGE_SIZE - 1);
    if (auto vp = va->LookupVAddrAndLock(alignedVirt); vp != nullptr) {
        const bool areaIsWritable = (va->va_flags & vm::flag::Write) != 0;
        if (isWriteFault && areaIsWritable) {
            // Page must be COW'ed - promote the page and re-map it. If this
            // changes the VMPage, AssignPageToVirtualAddress() will Deref()
            // the old one
            auto& new_vp = vp->Promote();
            AssignPageToVirtualAddress(*this, *va, interval, alignedVirt, new_vp);
            new_vp.Unlock();
            return Result::Success();
        }

        // Page is already mapped, but not COW. Bad, reject
        vp->Unlock();
        return Result::Failure(EFAULT);
    }

    // If there is a dentry attached here, perhaps we may find what we need in the corresponding
    // inode
    VMPage* new_vp = nullptr;
    if (va->va_dentry != nullptr) {
        new_vp = HandleDEntryBackedFault(*this, *va, interval, isWriteFault, alignedVirt);
    }
    if (new_vp == nullptr) {
        // We need a new VM page here; this is an anonymous mapping which we need to back
        new_vp = &vmpage::Allocate(0);
        new_vp->Zero(*this, *va, alignedVirt);
    }

    AssignPageToVirtualAddress(*this, *va, interval, alignedVirt, *new_vp);
    new_vp->Unlock();
    return Result::Success();
}
