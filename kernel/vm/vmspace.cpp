/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/vmarea.h"
#include "kernel/vmpage.h"
#include "kernel/vmspace.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/types.h"
#include "kernel/vm.h"
#include "kernel-md/param.h" // for THREAD_INITIAL_MAPPING_ADDR
#include "kernel-md/md.h"

namespace
{
    size_t BytesToPages(size_t len) { return (len + PAGE_SIZE - 1) / PAGE_SIZE; }

    addr_t RoundUpToPage(addr_t addr)
    {
        if ((addr & (PAGE_SIZE - 1)) == 0)
            return addr;

        return (addr | (PAGE_SIZE - 1)) + 1;
    }

    void FreeAllAreas(VMSpace& vs)
    {
        while (!vs.vs_areamap.empty()) {
            auto va = (*vs.vs_areamap.begin()).value;
            vs.FreeArea(*va);
        }
    }

    constexpr bool IsPageInRange(const VMPage& vp, const addr_t virt, const size_t len)
    {
        const auto pageAddr = vp.vp_vaddr;
        return pageAddr >= virt && (pageAddr + PAGE_SIZE) < (virt + len);
    }

    void MigratePagesToNewVA(VMArea& va, const VAInterval& interval, VMArea& newVA)
    {
        for (auto it = va.va_pages.begin(); it != va.va_pages.end(); /* nothing */) {
            auto& vp = *it;
            ++it;

            if (!IsPageInRange(vp, interval.begin, interval.end - interval.begin))
                continue;

            va.va_pages.remove(vp);
            vp.vp_vmarea = &newVA;
            newVA.va_pages.push_back(vp);
        }
    }

    void UpdateFileMappingOffset(VMArea& va, VMArea& newVA, const VAInterval& vaInterval, const VAInterval& newInterval)
    {
        if (va.va_dentry == nullptr)
            return; // no mapping, nothing to do
        newVA.va_dentry = va.va_dentry;
        dentry_ref(*newVA.va_dentry);

        kprintf("UpdateFileMappingOffset: vaInterval %p..%p doffset %p dlength %p\n",
            vaInterval.begin, vaInterval.end,
            va.va_doffset, va.va_dlength);

        const auto delta = newInterval.begin - vaInterval.begin;
        newVA.va_doffset = va.va_doffset + delta;
        newVA.va_dlength = va.va_dlength - delta;

        kprintf("UpdateFileMappingOffset: new va %p..%p ==> doffset %p dlength %p\n",
            newInterval.begin, newInterval.end, newVA.va_doffset, newVA.va_dlength);
    }

    void FreeRange(VMSpace& vs, const VAInterval& range_to_free)
    {
        for (auto [ vaInterval, va ]: vs.vs_areamap) {
            if (vaInterval == range_to_free) {
                // Interval matches as-if with what to free; this is easy
                vs.FreeArea(*va);
                return;
            }

            // See if there is any overlap
            const auto overlap = vaInterval.overlap(range_to_free);
            if (overlap.empty())
                continue; // not within our area, skip

            // Disconnect the matching va; this allows us to insert the new ranges
            vs.vs_areamap.remove(va);

            // Determine the new intervals if we'd free this range
            const auto interval_1 = VAInterval{ vaInterval.begin, overlap.begin };
            const auto interval_2 = VAInterval{ overlap.end, vaInterval.end };

            auto migrateToNewRange = [&](const auto& newInterval) {
                auto newVA = new VMArea(vs, newInterval.begin, newInterval.end - newInterval.begin, va->va_flags);
                MigratePagesToNewVA(*va, newInterval, *newVA);
                UpdateFileMappingOffset(*va, *newVA, vaInterval, newInterval);
                vs.vs_areamap.insert(newInterval, newVA);
            };

            if (!interval_1.empty()) {
                migrateToNewRange(interval_1);
            }
            if (!interval_2.empty()) {
                migrateToNewRange(interval_2);
            }

            // Throw the old va away, we've split it up as needed
            delete va;
            return;
        }
    }
} // unnamed namespace

addr_t VMSpace::ReserveAdressRange(size_t len)
{
    /*
     * XXX This is a bit of a kludge - besides, we currently never re-use old
     * addresses which may get ugly.
     */
    addr_t virt = vs_next_mapping;
    vs_next_mapping = RoundUpToPage(vs_next_mapping + len);
    return virt;
}

Result vmspace_create(VMSpace*& vmspace)
{
    auto vs = new VMSpace;
    vs->vs_next_mapping = THREAD_INITIAL_MAPPING_ADDR;

    if (auto result = md::vmspace::Init(*vs); result.IsFailure())
        return result;

    vmspace = vs;
    return Result::Success();
}

void vmspace_destroy(VMSpace& vs)
{
    FreeAllAreas(vs);

    /* Remove the vmspace-specific mappings - these are generally MD */
    for (auto it = vs.vs_pages.begin(); it != vs.vs_pages.end(); /* nothing */) {
        auto& p = *it;
        ++it;
        /* XXX should we unmap the page here? the vmspace shouldn't be active... */
        page_free(p);
    }
    md::vmspace::Destroy(vs);
    delete &vs;
}

Result VMSpace::Map(
    const VAInterval& vaInterval, const addr_t phys, const uint32_t areaFlags,
    const uint32_t mapFlags, VMArea*& va_out)
{
    if (vaInterval.empty())
        return Result::Failure(EINVAL);

    // Make sure the range is unused
    FreeRange(*this, vaInterval);

    /*
     * XXX We should ask the VM for some kind of reservation if the
     * THREAD_MAP_ALLOC flag is set; now we'll just assume that the
     * memory is there...
     */
    auto va = new VMArea(*this, vaInterval.begin, vaInterval.end - vaInterval.begin, areaFlags);
    //kprintf("Map: adding new vmspace for %p..%p -> %p, pre\n", virt, virt+len, va);
    vs_areamap.insert(vaInterval, va);
    va_out = va;

    /* Provide a mapping for the pages */
    md::vm::MapPages(*this, va->va_virt, phys, BytesToPages(vaInterval.length()), mapFlags);
    return Result::Success();
}

Result VMSpace::MapTo(const VAInterval& vaInterval, uint32_t flags, VMArea*& va_out)
{
    return Map(vaInterval, 0, flags, 0, va_out);
}

Result VMSpace::MapToDentry(
    const VAInterval& va, DEntry& dentry, const DEntryInterval& de, int flags,
    VMArea*& va_out)
{
    // Ensure the range we are mapping does not exceed the inode; if this is the case, we silently
    // truncate - but still up to a full page as we can't map anything less
    VAInterval vaInterval{ va };
    if (de.begin + vaInterval.length() > dentry.d_inode->i_sb.st_size) {
        vaInterval.end = vaInterval.begin + RoundUpToPage(dentry.d_inode->i_sb.st_size - de.begin);
    }

    KASSERT((de.begin & (PAGE_SIZE - 1)) == 0, "offset %d not page-aligned", de.begin);

    if (auto result = MapTo(vaInterval, flags | VM_FLAG_FAULT, va_out); result.IsFailure())
        return result;

    dentry_ref(dentry);
    va_out->va_dentry = &dentry;
    va_out->va_doffset = de.begin;
    va_out->va_dlength = de.length();
    return Result::Success();
}

Result VMSpace::Map(size_t len /* bytes */, uint32_t flags, VMArea*& va_out)
{
    const auto va_begin = ReserveAdressRange(len);
    const auto va_end = va_begin + len;
    return MapTo(VAInterval{ va_begin, va_end }, flags, va_out);
}

void VMSpace::PrepareForExecute()
{
    // Throw all non-MD mappings away - this should only leave the kernel stack in place
    for (auto it = vs_areamap.begin(); it != vs_areamap.end(); /* nothing */) {
        auto& va = *(it->value);
        if (va.va_flags & VM_FLAG_MD) {
            ++it;
            continue;
        }
        FreeArea(va);
        it = vs_areamap.begin();
    }
}

Result VMSpace::Clone(VMSpace& vs_dest)
{
    FreeAllAreas(vs_dest);

    /* Now copy everything over that isn't private */
    for (auto& [srcInterval, va_src ]: vs_areamap) {
        VMArea* va_dst;
        if (auto result = vs_dest.MapTo(srcInterval, va_src->va_flags, va_dst);
            result.IsFailure())
            return result;
        if (va_src->va_dentry != nullptr) {
            // Backed by an inode; copy the necessary fields over
            va_dst->va_doffset = va_src->va_doffset;
            va_dst->va_dlength = va_src->va_dlength;
            va_dst->va_dentry = va_src->va_dentry;
            dentry_ref(*va_dst->va_dentry);
        }

        // Copy the area page-wise
        for (auto& p : va_src->va_pages) {
            p.Lock();
            util::locked<VMPage> vp(p);
            KASSERT(
                vp->GetPage()->p_order == 0, "unexpected %d order page here",
                vp->GetPage()->p_order);

            // Create a clone of the data; it is up to the vmpage how to do this (it may go for COW)
            VMPage& new_vp = vmpage_clone(this, vs_dest, *va_src, *va_dst, vp);

            // Map the page into the cloned vmspace
            new_vp.Map(vs_dest, *va_dst);
            new_vp.Unlock();
            vp.Unlock();
        }
    }

    /*
     * See where the next mapping can be placed; we should use something more
     * clever than vs_next_mapping someday but this will have to suffice for
     * now.
     */
    vs_dest.vs_next_mapping = 0;
    for (auto& [ interval, va  ]: vs_dest.vs_areamap) {
        addr_t next = RoundUpToPage(va->va_virt + va->va_len);
        if (vs_dest.vs_next_mapping < next)
            vs_dest.vs_next_mapping = next;
    }

    return Result::Success();
}

void VMSpace::FreeArea(VMArea& va)
{
    vs_areamap.remove(&va);

    /* Free any backing dentry, if we have one */
    if (va.va_dentry != nullptr)
        dentry_deref(*va.va_dentry);

    /*
     * If the pages were allocated, we need to free them one by one
     *
     * Note that this changes va_pages while we are iterating through it, so we
     * need to increment the iterator beforehand!
     */
    for (auto it = va.va_pages.begin(); it != va.va_pages.end(); /* nothing */) {
        VMPage& vp = *it;
        ++it;

        vp.Lock();
        KASSERT(vp.vp_vmarea == &va, "wrong vmarea");
        vp.vp_vmarea = nullptr;
        vp.Deref();
    }
    delete &va;
}

void VMSpace::Dump()
{
    for (auto& [ interval, va ]: vs_areamap) {
        kprintf(
            "  area %p: %p..%p flags %c%c%c%c%c%c%c\n", &va, va->va_virt, va->va_virt + va->va_len - 1,
            (va->va_flags & VM_FLAG_READ) ? 'r' : '.', (va->va_flags & VM_FLAG_WRITE) ? 'w' : '.',
            (va->va_flags & VM_FLAG_EXECUTE) ? 'x' : '.', (va->va_flags & VM_FLAG_KERNEL) ? 'k' : '.',
            (va->va_flags & VM_FLAG_USER) ? 'u' : '.', (va->va_flags & VM_FLAG_PRIVATE) ? 'p' : '.',
            (va->va_flags & VM_FLAG_MD) ? 'm' : '.');
        kprintf("    pages:\n");
        for (auto& vp : va->va_pages) {
            vp.Dump("      ");
        }
    }
}
