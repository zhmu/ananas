/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include <ananas/util/vector.h>

struct DEntry;
struct VMPage;
struct VMSpace;

// Note: these flags are shared with VM_FLAG_...
namespace vmarea::flag
{
    inline constexpr auto COW = (1 << 17); // pages must be copied on write
}

/*
 * VM area describes an adjacent mapping though virtual memory. It can be
 * backed by an inode in the following way:
 *
 *                         +------+
 *              va_doffset |      |
 *       +-------+\      \ |      |
 *       |       | \      \|      |
 *       |       |  -------+......| ^
 *       |       |         | file | | va_dlength
 *       +-------+         |      | |
 *       |       |<--------+......| v
 *       | page2 |         |      |
 *       |       |         |      |
 *       +-------+         |      |
 *                         +------+
 *
 * Note that we'll only fault one page at a time.
 *
 */
struct VMArea final : util::List<VMArea>::NodePtr {
    VMArea(VMSpace& vs, addr_t virt, size_t len, int flags);
    ~VMArea();

    VMPage* LookupVAddrAndLock(addr_t vaddr);
    VMPage& AllocatePrivatePage(int flags);
    VMPage& PromotePage(VMPage& vp);

    VMSpace& va_vs;
    const unsigned int va_flags;    // flags, combination of VM_FLAG_...
    const addr_t va_virt;           // userland address
    const size_t va_len;            // length
    util::vector<VMPage*> va_pages; // backing pages
    /* dentry-specific mapping fields */
    DEntry* va_dentry = nullptr; /* backing dentry, if any */
    off_t va_doffset = 0;        /* dentry offset */
    size_t va_dlength = 0;       /* dentry length */
};
typedef util::List<VMArea> VMAreaList;
