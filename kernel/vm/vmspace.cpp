/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/process.h"
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
        for (auto [ vaInterval, va ]: vs.vs_areamap) {
            delete va;
        }
        vs.vs_areamap.clear();
    }

    void VerifyDentryOffset(VMArea& va)
    {
        KASSERT(
            (va.va_doffset & (PAGE_SIZE - 1)) == 0, "doffset %x not page-aligned",
            (int)va.va_doffset);
    }

    void MigratePagesToNewVA(VMArea& va, const VAInterval& interval, VMArea& newVA)
    {
        addr_t currentVa = interval.begin;
        for(size_t page_index = 0; page_index < va.va_pages.size(); ++page_index, currentVa += PAGE_SIZE) {
            auto& vp = va.va_pages[page_index];
            if (vp == nullptr || !interval.contains(currentVa)) {
                continue;
            }

            newVA.va_pages[(currentVa - newVA.va_virt) / PAGE_SIZE] = vp;
            vp = nullptr;
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
        VerifyDentryOffset(newVA);

        kprintf("UpdateFileMappingOffset: new va %p..%p ==> doffset %p dlength %p\n",
            newInterval.begin, newInterval.end, newVA.va_doffset, newVA.va_dlength);
    }

    void FreeRange(VMSpace& vs, const VAInterval& range_to_free)
    {
        for (auto [ vaInterval, va ]: vs.vs_areamap) {
            if (vaInterval == range_to_free) {
                // Interval matches as-if with what to free; this is easy
                vs.vs_areamap.remove(va);
                delete va;
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
                auto newVA = new VMArea(newInterval.begin, newInterval.end - newInterval.begin, va->va_flags);
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

    KASSERT(!vs.IsCurrent(), "destroying active vmspace");
    while(!vs.vs_md_pages.empty()) {
        auto& page = vs.vs_md_pages.front();
        vs.vs_md_pages.pop_front();

        page_free(page);
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
    auto va = new VMArea(vaInterval.begin, vaInterval.end - vaInterval.begin, areaFlags);

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
    KASSERT((de.begin & (PAGE_SIZE - 1)) == 0, "offset %d not page-aligned", de.begin);

    // Ensure the range we are mapping does not exceed the inode; if this is the case, we silently
    // truncate - but still up to a full page as we can't map anything less
    VAInterval vaInterval{ va };
    if (de.begin + vaInterval.length() > dentry.d_inode->i_sb.st_size) {
        vaInterval.end = vaInterval.begin + RoundUpToPage(dentry.d_inode->i_sb.st_size - de.begin);
    }

    if (auto result = MapTo(vaInterval, flags, va_out); result.IsFailure())
        return result;

    dentry_ref(dentry);
    va_out->va_dentry = &dentry;
    va_out->va_doffset = de.begin;
    va_out->va_dlength = de.length();
    VerifyDentryOffset(*va_out);
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
    // The kernel stack isn't part of this VMSpace, so we can simply throw away
    // all areas and start from a clean state. The userland stack will be
    // allocated by ELF64Loader::PrepareForExecute() as needed
    FreeAllAreas(*this);
}

void VMSpace::FreeArea(VMArea& va)
{
    const VAInterval interval{ va.va_virt, va.va_virt + va.va_len };
    vs_areamap.remove(interval);
    delete &va;
}

Result VMSpace::Clone(VMSpace& vs_dest)
{
    FreeAllAreas(vs_dest);
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
            VerifyDentryOffset(*va_dst);
            dentry_ref(*va_dst->va_dentry);
        }

        // Clone the area by sharing all VMPage's therein
        KASSERT(va_src->va_pages.size() == va_dst->va_pages.size(), "va_pages.size() mismatch");
        auto currentVa = srcInterval.begin;
        const bool vaIsPrivate = (va_src->va_flags & vm::flag::Private) != 0;
        const bool vaIsWritable = (va_src->va_flags & vm::flag::Write) != 0;
        for(size_t page_index = 0; page_index < va_src->va_pages.size(); ++page_index, currentVa += PAGE_SIZE) {
            auto vp = va_src->va_pages[page_index];
            if (vp == nullptr) continue;

            vp->Lock();
            KASSERT(
                vp->GetPage()->p_order == 0, "unexpected %d order page here",
                vp->GetPage()->p_order);

            // If the page is promoted within a private or writable area, we
            // need to COW it
            const bool isPromoted = (vp->vp_flags & vmpage::flag::Promoted) != 0;
            if ((vaIsPrivate || vaIsWritable) && isPromoted) {
                vp->vp_flags &= ~vmpage::flag::Promoted;
                vp->Map(*this, *va_src, currentVa);
            }

            // Hook the page up to the destination VMArea; this shares it
            // between the parent and child
            vp->Ref();
            KASSERT(vp->vp_refcount >= 2, "shared page has invalid refcount %d", vp->vp_refcount);
            va_dst->va_pages[page_index] = vp;
            vp->Map(vs_dest, *va_dst, currentVa);
            vp->Unlock();
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

void VMSpace::Dump()
{
    for (auto& [ interval, va ]: vs_areamap) {
        kprintf(
            "  area %p: %p..%p flags %c%c%c%c%c%c\n", &va, va->va_virt, va->va_virt + va->va_len - 1,
            (va->va_flags & vm::flag::Read) ? 'r' : '.', (va->va_flags & vm::flag::Write) ? 'w' : '.',
            (va->va_flags & vm::flag::Execute) ? 'x' : '.', (va->va_flags & vm::flag::Kernel) ? 'k' : '.',
            (va->va_flags & vm::flag::User) ? 'u' : '.', (va->va_flags & vm::flag::Private) ? 'p' : '.');
        kprintf("    pages:\n");
        for (auto vp : va->va_pages) {
            if (vp != nullptr) vp->Dump("      ");
        }
    }
}

bool VMSpace::IsCurrent() const
{
    return &process::GetCurrent().p_vmspace == this;
}
