/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"
#include <ananas/util/list.h>
#include <ananas/util/locked.h>
#include <machine/param.h>
#include "kernel/lock.h"
#include "kernel/refcount.h"

struct Page;

namespace vmpage::flag {
    inline constexpr auto Pending = (1 << 0); // Page is pending a read
    inline constexpr auto Promoted = (1 << 1); // Page is writable (used for COW)
    inline constexpr auto Inode = (1 << 2); // Page belongs to an inode
}

class VMArea;
class VMSpace;
struct INode;

struct VMPage final {
    Mutex vp_mtx{"vmpage"};
    refcount_t vp_refcount = 1; // instantiator
    int vp_flags;
    const off_t vp_offset; // offset within inode
    Page* vp_page{nullptr}; // backing page

    VMPage(int flags, off_t offset)
        : vp_flags(flags), vp_offset(offset)
    {
    }
    ~VMPage();

    void Ref();
    void Deref();
    Page* GetPage() { return vp_page; }

    // Promote a COW page to a new writable page; returns the (new) page to use
    VMPage& Promote();

    void Lock() { vp_mtx.Lock(); }

    void Unlock() { vp_mtx.Unlock(); }

    void AssertLocked();

    void Map(VMSpace&, VMArea&, addr_t virt);
    void Zero(VMSpace&, VMArea&, addr_t virt);
    void Dump(const char* prefix) const;

    VMPage& Duplicate(size_t copy_length);
};

namespace vmpage
{
    VMPage& Allocate(int flags);

    util::locked<VMPage> LookupOrCreateINodePage(INode& inode, off_t offs, int flags);
}
