/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <machine/param.h>
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "kernel-md/md.h"
#include "kernel-md/param.h"
#include "kernel-md/vm.h"

extern Page* usupport_page;

namespace vm_flag = vm::flag;

namespace md::vmspace
{
    Result Init(VMSpace& vs)
    {
        Page* pagedir_page;
        vs.vs_md_pagedir = static_cast<uint64_t*>(
            page_alloc_single_mapped(pagedir_page, vm_flag::Read | vm_flag::Write));
        if (vs.vs_md_pagedir == nullptr)
            return Result::Failure(ENOMEM);
        vs.vs_md_pages.push_back(*pagedir_page);

        /* Map the kernel pages in there */
        memset(vs.vs_md_pagedir, 0, PAGE_SIZE);
        md::vm::MapKernelSpace(vs);

        // Map the userland support page
        md::vm::MapPages(
            vs, USERLAND_SUPPORT_ADDR, usupport_page->GetPhysicalAddress(), 1,
            vm_flag::User | vm_flag::Read | vm_flag::Execute);
        return Result::Success();
    }

    void Destroy(VMSpace& vs) {}

} // namespace md::vmspace
