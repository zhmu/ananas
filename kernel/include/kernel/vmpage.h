/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include <ananas/util/locked.h>
#include <machine/param.h>
#include "kernel/lock.h"

struct Page;

namespace vmpage::flag {
    inline constexpr auto Private = (1 << 0);  // Page is private to the process
    inline constexpr auto ReadOnly = (1 << 1); // Page cannot be modified
    inline constexpr auto Pending = (1 << 2); // Page is pending a read
    inline constexpr auto Promoted = (1 << 3); // XXX this is for debugging only
}

class VMArea;
class VMSpace;
struct INode;

struct VMPage final {
    VMPage(INode& inode, off_t offset, int flags)
        : vp_inode(&inode), vp_offset(offset), vp_flags(flags)
    {
    }

    VMPage(int flags)
        : vp_flags(flags)
    {
    }

    int vp_flags;
    refcount_t vp_refcount = 1; // instantiator

    /* Backing page */
    Page* vp_page = nullptr;

    /* Backing inode and offset */
    INode* vp_inode = nullptr;
    off_t vp_offset{};

    void Ref();
    void Deref();
    Page* GetPage() { return vp_page; }

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

    void Map(VMArea&, addr_t virt);
    void Zero(VMArea&, addr_t virt);
    void Dump(const char* prefix) const;

  private:
    ~VMPage();
    Mutex vp_mtx{"vmpage"};
};

util::locked<VMPage> vmpage_lookup_locked(VMArea& va, INode& inode, off_t offs);
util::locked<VMPage> vmpage_create_shared(INode& inode, off_t offs, int flags);

VMPage& vmpage_clone(
    VMSpace& vs_source, VMSpace& vs_dest, VMArea& va_source, VMArea& va_dest,
    util::locked<VMPage>& vp_orig);

VMPage& vmpage_clone_dentry_page(VMSpace& vs_dest, VMArea& va, util::locked<VMPage>& vmpage);
