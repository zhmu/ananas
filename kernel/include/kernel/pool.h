/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/list.h>
#include "kernel/lock.h"
#include "kernel/page.h"

namespace pool
{
    // Page-backed memory pool with elements of a fixed size
    struct Pool final : util::List<Pool>::NodePtr
    {
        Pool(const char*, size_t);
        ~Pool();

        struct Item : util::List<Item>::NodePtr
        {
        };

        Mutex p_Mutex;
        char p_Name[16];
        PageList p_AllocatedPages;
        util::List<Item> p_AvailableItems;
        const size_t p_ItemSize{};

        void* AllocateItem();
        void FreeItem(void*);

    private:
        void Grow();
    };
}
