/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include <ananas/util/interval_map.h>
#include "kernel/page.h"
#include "kernel/vmpage.h"
#include "kernel-md/vmspace.h"

struct DEntry;
struct VMArea;
class Result;

using VAInterval = util::interval<addr_t>;
using DEntryInterval = util::interval<off_t>;

/*
 * VM space describes a thread's complete overview of memory.
 */
struct VMSpace {
    MD_VMSPACE_FIELDS

    void Lock() { vs_mutex.Lock(); }

    void Unlock() { vs_mutex.Unlock(); }

    void AssertLocked() { vs_mutex.AssertLocked(); }

    util::interval_map<addr_t, VMArea*> vs_areamap;

    /*
     * Contains pages allocated to the space that aren't part of a mapping; this
     * is mainly used to store MD-dependant things like pagetables.
     */
    PageList vs_pages;

    addr_t vs_next_mapping; /* address of next mapping */

    Result Map(const VAInterval&, addr_t phys, uint32_t areaFlags, uint32_t mapFlags, VMArea*& va_out);
    Result MapTo(const VAInterval&, uint32_t flags, VMArea*& va_out);
    Result MapToDentry(
        const VAInterval&, DEntry& dentry, const DEntryInterval&, int flags,
        VMArea*& va_out);
    Result Map(size_t len /* bytes */, uint32_t flags, VMArea*& va_out);
    Result Clone(VMSpace& vs_dest);
    void Dump();

    addr_t ReserveAdressRange(size_t len);
    Result HandleFault(addr_t virt, int flags);

    void PrepareForExecute();

    // XXX Should not be public
    void FreeArea(VMArea& va);

  private:
    Mutex vs_mutex{"vmspace"}; /* protects all fields and sub-areas */
};

Result vmspace_create(VMSpace*& vs);
void vmspace_destroy(VMSpace& vs);
