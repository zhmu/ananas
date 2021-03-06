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
    constexpr auto debugVASplit = false;

    size_t BytesToPages(size_t len) { return (len + PAGE_SIZE - 1) / PAGE_SIZE; }

    addr_t RoundUp(addr_t addr)
    {
        if ((addr & (PAGE_SIZE - 1)) == 0)
            return addr;

        return (addr | (PAGE_SIZE - 1)) + 1;
    }

} // unnamed namespace

addr_t VMSpace::ReserveAdressRange(size_t len)
{
    /*
     * XXX This is a bit of a kludge - besides, we currently never re-use old
     * addresses which may get ugly.
     */
    addr_t virt = vs_next_mapping;
    vs_next_mapping = RoundUp(vs_next_mapping + len);
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

static void vmspace_cleanup(VMSpace& vs)
{
    /* Cleanup only removes all mapped areas */
    while (!vs.vs_areas.empty()) {
        VMArea& va = vs.vs_areas.front();
        vs.FreeArea(va);
    }
}

void vmspace_destroy(VMSpace& vs)
{
    /* Ensure all mapped areas are gone (can't hurt if this is already done) */
    vmspace_cleanup(vs);

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

namespace {
    constexpr bool IsPageInRange(const VMPage& vp, const addr_t virt, const size_t len)
    {
        const auto pageAddr = vp.vp_vaddr;
        return pageAddr >= virt && (pageAddr + PAGE_SIZE) < (virt + len);
    }
}

static bool vmspace_free_range(VMSpace& vs, addr_t virt, size_t len)
{
    for (auto& va : vs.vs_areas) {
        /*
         * To keep things simple, we will only break up mappings if they occur
         * completely within a single - this avoids complicated merging logic.
         */
        if (virt >= va.va_virt + va.va_len || virt + len <= va.va_virt)
            continue; // not within our area, skip

        // See if this range extends before or beyond our va - if that is the case, we
        // will reject it
        if (virt < va.va_virt || virt + len > va.va_virt + va.va_len)
            return false;

        // Okay, we can alter this va to exclude our mapping. If it matches 1-to-1, just throw it
        // away
        if (virt == va.va_virt && len == va.va_len) {
            vs.FreeArea(va);
            return true;
        }

        // Not a full match; maybe at the start?
        if (virt == va.va_virt) {
            // Shift the entire range, but ensure we'll honor page-boundaries
            va.va_virt = RoundUp(va.va_virt + len);
            va.va_len = RoundUp(va.va_len - len);
            if (va.va_dentry != nullptr) {
                // Backed by an inode; correct the offsets
                va.va_doffset += len;
                if (va.va_dlength >= len)
                    va.va_dlength -= len;
                else
                    va.va_dlength = 0;
                KASSERT(
                    (va.va_doffset & (PAGE_SIZE - 1)) == 0, "doffset %x not page-aligned",
                    (int)va.va_doffset);
            }

            // Make sure we will have space for our next mapping XXX
            addr_t next_addr = va.va_virt + va.va_len;
            if (vs.vs_next_mapping < next_addr)
                vs.vs_next_mapping = next_addr;
            return true;
        }

        // XXX we need to cover the other use cases here as well (range at end, range in the middle)
        const auto va_start_1 = va.va_virt;
        const auto va_len_1 = virt - va.va_virt;
        const auto va_start_2 = virt + len;
        const auto va_len_2 = (va.va_virt + va.va_len) - (virt + len);
        if constexpr (debugVASplit) {
            kprintf("va_1 = %p .. %p (len %p)\n", va_start_1, va_start_1 + va_len_1, va_len_1);
            kprintf("va_2 = %p .. %p (len %p)\n", va_start_2, va_start_2 + va_len_2, va_len_2);
        }

        // Move all pages that belong to VA. Free all that belong to the new range
        auto newVA = new VMArea(vs, va_start_2, va_len_2, va.va_flags);
        for (auto it = va.va_pages.begin(); it != va.va_pages.end(); /* nothing */) {
            VMPage& vp = *it;
            ++it;

            if (IsPageInRange(vp, va_start_2, va_len_2)) {
                if constexpr (debugVASplit) {
                    kprintf("moving page %p to range 2\n", vp.vp_vaddr);
                }
                newVA->va_pages.push_back(vp);
                continue;
            }
            if (IsPageInRange(vp, virt, len)) {
                va.va_pages.remove(vp);
                if constexpr (debugVASplit) {
                    kprintf("freeing page %p\n", vp.vp_vaddr);
                }
                continue;
            }
            KASSERT(IsPageInRange(vp, va_start_1, va_len_1), "huh %p", vp.vp_vaddr);
        }

        if (va.va_dentry != nullptr) {
            newVA->va_dentry = va.va_dentry;
            dentry_ref(*newVA->va_dentry);

            if constexpr (debugVASplit) {
                kprintf("adjusting va1 len %p -> %p\n",
                    va.va_dlength, va.va_dlength - (va.va_len - va_len_1));
            }
            va.va_dlength -= (va.va_len - va_len_1);
            if (va.va_dlength < 0) va.va_dlength = 0;
            if constexpr (debugVASplit) {
                kprintf("va1 doffset %p dlen %d\n", va.va_doffset, va.va_dlength);
            }

            newVA->va_doffset = va.va_doffset + (va_start_2 - va_start_1);
            newVA->va_dlength = va.va_dlength - (va_start_2 - va_start_1);

            if constexpr (debugVASplit) {
                kprintf("va2 doffset %p dlen %d\n", newVA->va_doffset, newVA->va_dlength);
            }
        }
        va.va_virt = va_start_1;
        va.va_len = va_len_1;
        return true;
    }

    // No ranges match; this is okay
    return true;
}

Result VMSpace::Map(
    const addr_t virt, const addr_t phys, const size_t len /* bytes */, const uint32_t areaFlags,
    const uint32_t mapFlags, VMArea*& va_out)
{
    if (len == 0)
        return Result::Failure(EINVAL);

    // If the virtual address space is already in use, we need to break it up
    if (!vmspace_free_range(*this, virt, len)) {
        kprintf("range %p..%p not free!!\n", virt, virt + len);
        Dump();
        return Result::Failure(ENOSPC);
    }

    /*
     * XXX We should ask the VM for some kind of reservation if the
     * THREAD_MAP_ALLOC flag is set; now we'll just assume that the
     * memory is there...
     */
    auto va = new VMArea(*this, virt, len, areaFlags);
    vs_areas.push_back(*va);
    va_out = va;

    /* Provide a mapping for the pages */
    md::vm::MapPages(*this, va->va_virt, phys, BytesToPages(len), mapFlags);
    return Result::Success();
}

Result VMSpace::MapTo(addr_t virt, size_t len /* bytes */, uint32_t flags, VMArea*& va_out)
{
    return Map(virt, 0, len, flags, 0, va_out);
}

Result VMSpace::MapToDentry(
    addr_t virt, size_t vlength, DEntry& dentry, off_t doffset, size_t dlength, int flags,
    VMArea*& va_out)
{
    // Ensure the range we are mapping does not exceed the inode; if this is the case, we silently
    // truncate - but still up to a full page as we can't map anything less
    if (doffset + vlength > dentry.d_inode->i_sb.st_size) {
        vlength = RoundUp(dentry.d_inode->i_sb.st_size - doffset);
    }

    KASSERT((doffset & (PAGE_SIZE - 1)) == 0, "offset %d not page-aligned", doffset);

    if (auto result = MapTo(virt, vlength, flags | VM_FLAG_FAULT, va_out); result.IsFailure())
        return result;

    dentry_ref(dentry);
    va_out->va_dentry = &dentry;
    va_out->va_doffset = doffset;
    va_out->va_dlength = dlength;
    return Result::Success();
}

Result VMSpace::Map(size_t len /* bytes */, uint32_t flags, VMArea*& va_out)
{
    const auto va = ReserveAdressRange(len);
    return MapTo(va, len, flags, va_out);
}

void VMSpace::PrepareForExecute()
{
    // Throw all non-MD mappings away - this should only leave the kernel stack in place
    for (auto it = vs_areas.begin(); it != vs_areas.end(); /* nothing */) {
        VMArea& va = *it;
        ++it;
        if (va.va_flags & VM_FLAG_MD)
            continue;
        FreeArea(va);
    }
}

Result VMSpace::Clone(VMSpace& vs_dest)
{
    /*
     * First, clean up the destination area's mappings - this ensures we'll
     * overwrite them with our own. Note that we'll leave private mappings alone.
     *
     * Note that this changes vs_areas while we are iterating through it, so we
     * need to increment the iterator beforehand!
     */
    for (auto it = vs_dest.vs_areas.begin(); it != vs_dest.vs_areas.end(); /* nothing */) {
        VMArea& va = *it;
        ++it;
        vs_dest.FreeArea(va);
    }

    /* Now copy everything over that isn't private */
    for (auto& va_src : vs_areas) {
        VMArea* va_dst;
        if (auto result = vs_dest.MapTo(va_src.va_virt, va_src.va_len, va_src.va_flags, va_dst);
            result.IsFailure())
            return result;
        if (va_src.va_dentry != nullptr) {
            // Backed by an inode; copy the necessary fields over
            va_dst->va_doffset = va_src.va_doffset;
            va_dst->va_dlength = va_src.va_dlength;
            va_dst->va_dentry = va_src.va_dentry;
            dentry_ref(*va_dst->va_dentry);
        }

        // Copy the area page-wise
        for (auto& p : va_src.va_pages) {
            p.Lock();
            util::locked<VMPage> vp(p);
            KASSERT(
                vp->GetPage()->p_order == 0, "unexpected %d order page here",
                vp->GetPage()->p_order);

            // Create a clone of the data; it is up to the vmpage how to do this (it may go for COW)
            VMPage& new_vp = vmpage_clone(this, vs_dest, va_src, *va_dst, vp);

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
    for (auto& va : vs_dest.vs_areas) {
        addr_t next = RoundUp(va.va_virt + va.va_len);
        if (vs_dest.vs_next_mapping < next)
            vs_dest.vs_next_mapping = next;
    }

    return Result::Success();
}

void VMSpace::FreeArea(VMArea& va)
{
    vs_areas.remove(va);

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
    for (auto& va : vs_areas) {
        kprintf(
            "  area %p: %p..%p flags %c%c%c%c%c%c%c\n", &va, va.va_virt, va.va_virt + va.va_len - 1,
            (va.va_flags & VM_FLAG_READ) ? 'r' : '.', (va.va_flags & VM_FLAG_WRITE) ? 'w' : '.',
            (va.va_flags & VM_FLAG_EXECUTE) ? 'x' : '.', (va.va_flags & VM_FLAG_KERNEL) ? 'k' : '.',
            (va.va_flags & VM_FLAG_USER) ? 'u' : '.', (va.va_flags & VM_FLAG_PRIVATE) ? 'p' : '.',
            (va.va_flags & VM_FLAG_MD) ? 'm' : '.');
        kprintf("    pages:\n");
        for (auto& vp : va.va_pages) {
            vp.Dump("      ");
        }
    }
}
