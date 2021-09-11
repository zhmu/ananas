/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/userland.h"
#include "kernel/kmem.h"
#include "kernel/result.h"
#include "kernel/vmspace.h"
#include "kernel/vmarea.h"
#include "kernel/vmpage.h"
#include "kernel/vm.h"
#include "kernel-md/param.h"

namespace userland
{
    VMPage& CreateStack(VMSpace& vs, const addr_t stackEnd)
    {
        VMArea* va;
        vs.MapTo(
            VAInterval{ USERLAND_STACK_ADDR, stackEnd },
            vm::flag::User | vm::flag::Read | vm::flag::Write | vm::flag::Private, va);

        // Pre-fault the first page so that we can put stuff in it
        auto stackPageVirt = stackEnd - PAGE_SIZE;
        auto& stack_vp = vmpage::Allocate(0);
        const auto page_index = (stackPageVirt  - USERLAND_STACK_ADDR) / PAGE_SIZE;
        KASSERT(page_index < va->va_pages.size(), "oeps %d %d", page_index, va->va_pages.size());
        va->va_pages[page_index] = &stack_vp;
        stack_vp.Map(vs, *va, stackPageVirt);
        return stack_vp;
    }

    VMPage& CreateCode(VMSpace& vs, const addr_t virt, const addr_t len)
    {
        // Create code
        VMArea* vaCode;
        vs.MapTo(
            VAInterval{ virt, virt + len },
            vm::flag::User | vm::flag::Read | vm::flag::Write | vm::flag::Private | vm::flag::Execute, vaCode);

        // Map code in place
        auto& code_vp = vmpage::Allocate(0);
        vaCode->va_pages[0] = &code_vp;
        code_vp.Map(vs, *vaCode, virt);
        return code_vp;
    }

    void* MapPageToKernel(VMPage& vp)
    {
        return kmem_map(
            vp.GetPage()->GetPhysicalAddress(), PAGE_SIZE, vm::flag::Read | vm::flag::Write);
    }

    void UnmapPage(void* p)
    {
        kmem_unmap(p, PAGE_SIZE);
    }
}
