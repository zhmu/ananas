/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <machine/param.h>
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "kernel-md/vm.h"

namespace md::vm
{
    namespace
    {
        auto get_nextpage(VMSpace& vs, uint64_t page_flags)
        {
            Page* p = page_alloc_single();
            KASSERT(p != NULL, "out of pages");

            /*
             * If the page isn't mapped globally, it belongs to a thread and we should
             * administer it there so we can free it once the thread is freed.
             */
            if ((page_flags & PE_C_G) == 0)
                vs.vs_md_pages.push_back(*p);

            /* Map this page in kernel-space XXX How do we clean it up? */
            addr_t phys = p->GetPhysicalAddress();
            void* va = kmem_map(phys, PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
            memset(va, 0, PAGE_SIZE);
            return phys | page_flags;
        }

        inline uint64_t* pt_resolve_addr(uint64_t entry)
        {
            constexpr addr_t addressMask = 0xffffffffff000; // bits 12 .. 51
            return (uint64_t*)(KMEM_DIRECT_VA_START + (entry & addressMask));
        }

    } // unnamed namespace

    void MapPages(VMSpace& vs, addr_t virt, addr_t phys, size_t num_pages, int flags)
    {
        /* Flags for the mapped pages themselves */
        uint64_t pt_flags = 0;
        if (flags & VM_FLAG_READ)
            pt_flags |= PE_P; /* XXX */
        if (flags & VM_FLAG_USER)
            pt_flags |= PE_US;
        if (flags & VM_FLAG_WRITE)
            pt_flags |= PE_RW;
        if (flags & VM_FLAG_DEVICE)
            pt_flags |= PE_PCD | PE_PWT;
        if ((flags & VM_FLAG_EXECUTE) == 0)
            pt_flags |= PE_NX;

        /* Flags for the page-directory leading up to the mapped page */
        uint64_t pd_flags = PE_US | PE_P | PE_RW;

        /* XXX we don't yet strip off bits 52-63 yet */
        auto pagedir = vs.vs_md_pagedir;
        while (num_pages--) {
            if (pagedir[(virt >> 39) & 0x1ff] == 0) {
                pagedir[(virt >> 39) & 0x1ff] = get_nextpage(vs, pd_flags);
            }

            /*
             * XXX We only look at the top level pagetable flags to determine whether
             *     the page should be mapped globally - the idea is that all ranges
             *     (KVA, kernel) where this should happen are pre-allocated in startup.c
             *     and thus thee is no need to look further...
             */
            if (pagedir[(virt >> 39) & 0x1ff] & PE_C_G) {
                pd_flags |= PE_C_G;
                pt_flags |= PE_G;
            }

            uint64_t* pdpe = pt_resolve_addr(pagedir[(virt >> 39) & 0x1ff]);
            if (pdpe[(virt >> 30) & 0x1ff] == 0) {
                pdpe[(virt >> 30) & 0x1ff] = get_nextpage(vs, pd_flags);
            }

            uint64_t* pde = pt_resolve_addr(pdpe[(virt >> 30) & 0x1ff]);
            if (pde[(virt >> 21) & 0x1ff] == 0) {
                pde[(virt >> 21) & 0x1ff] = get_nextpage(vs, pd_flags);
            }

            // Ensure we'll flush the mapping if it was already present - it may be in the TLB
            uint64_t* pte = pt_resolve_addr(pde[(virt >> 21) & 0x1ff]);
            bool need_invalidate = (pte[(virt >> 12) & 0x1ff] & PE_P) != 0;
            pte[(virt >> 12) & 0x1ff] = (uint64_t)phys | pt_flags;
            if (need_invalidate)
                __asm __volatile("invlpg %0" : : "m"(*(char*)virt) : "memory");

            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
        }
    }

    void UnmapPages(VMSpace& vs, addr_t virt, size_t num_pages)
    {
        const auto& process = process::GetCurrent();
        const bool is_cur_vmspace = &process.p_vmspace == &vs;

        /* XXX we don't yet strip off bits 52-63 yet */
        auto pagedir = vs.vs_md_pagedir;
        while (num_pages--) {
            if (pagedir[(virt >> 39) & 0x1ff] == 0) {
                panic(
                    "vs=%p, virt=%p -> l1 not mapped (%p)", &vs, virt,
                    pagedir[(virt >> 39) & 0x1ff]);
            }

            uint64_t* pdpe = pt_resolve_addr(pagedir[(virt >> 39) & 0x1ff]);
            if (pdpe[(virt >> 30) & 0x1ff] == 0) {
                panic(
                    "vs=%p, virt=%p -> l2 not mapped (%p)", &vs, virt,
                    pagedir[(virt >> 30) & 0x1ff]);
            }

            uint64_t* pde = pt_resolve_addr(pdpe[(virt >> 30) & 0x1ff]);
            if (pde[(virt >> 21) & 0x1ff] == 0) {
                panic(
                    "vs=%p, virt=%p -> l3 not mapped (%p)", &vs, virt,
                    pagedir[(virt >> 21) & 0x1ff]);
            }

            /* XXX perhaps we should check if this is actually mapped */
            uint64_t* pte = pt_resolve_addr(pde[(virt >> 21) & 0x1ff]);
            int global = (pte[(virt >> 12) & 0x1ff] & PE_G);
            pte[(virt >> 12) & 0x1ff] = 0;
            if (global || is_cur_vmspace) {
                /*
                 * We just unmapped a global virtual address or something that belongs to
                 * the current thread; this means we'll have to * explicitely invalidate it.
                 */
                __asm __volatile("invlpg %0" : : "m"(*(char*)virt) : "memory");
            }
            virt += PAGE_SIZE;
        }
    }

    void MapKernel(addr_t phys, addr_t virt, size_t num_pages, int flags)
    {
        MapPages(*kernel_vmspace, virt, phys, num_pages, flags);
    }

    void UnmapKernel(addr_t virt, size_t num_pages) { UnmapPages(*kernel_vmspace, virt, num_pages); }

    void MapKernelSpace(VMSpace& vs)
    {
        /* We can just copy the entire kernel pagemap over; it's shared with everything else */
        memcpy(vs.vs_md_pagedir, kernel_vmspace->vs_md_pagedir, PAGE_SIZE);
    }

} // namespace md::vm
