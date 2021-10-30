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
}

class VMArea;
class VMSpace;
struct INode;

struct VMPage final {
    VMPage(off_t offset, int flags)
        : vp_offset(offset), vp_flags(flags)
    {
    }

    VMPage(int flags)
        : vp_flags(flags)
    {
    }

    int vp_flags;
    Page* vp_page = nullptr; // backing page
    const off_t vp_offset{}; // offset within inode

    void Ref();
    void Deref();
    Page* GetPage() { return vp_page; }

    // Promote a COW page to a new writable page; returns the (new) page to use
    VMPage& Promote();

    /*
     * Copies a (piece of) vp_src to vp_dst:
     *
     *        len
     *         /
     * +-------+------+
     * |XXXXXXX|??????|
     * +-------+------+
     *         |
     *         v
     * +-------+---+
     * |XXXXXXX|000|
     * +-------+---+
     *         ^    \
     *        len    PAGE_SIZE
     *
     *
     * X = bytes to be copied, 0 = bytes set to zero, ? = don't care - note that
     * thus the vp_dst page is always completely filled.
     */
    void CopyExtended(VMPage& vp_dst, size_t len);

    void Copy(VMPage& vp_dst) { CopyExtended(vp_dst, PAGE_SIZE); }

    void Lock() { vp_mtx.Lock(); }

    void Unlock() { vp_mtx.Unlock(); }

    void AssertLocked();

    void Map(VMSpace&, VMArea&, addr_t virt);
    void Zero(VMSpace&, VMArea&, addr_t virt);
    void Dump(const char* prefix) const;

    VMPage& Clone(VMSpace&, VMArea& va_source, addr_t virt);
    VMPage& Duplicate();

  private:
    ~VMPage();

    Mutex vp_mtx{"vmpage"};
    refcount_t vp_refcount = 1; // instantiator
};

namespace vmpage
{
    VMPage& Allocate(int flags);

    util::locked<VMPage> LookupOrCreateINodePage(INode& inode, off_t offs, int flags);
}
