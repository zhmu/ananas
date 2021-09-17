/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/pool.h"
#include "kernel/lib.h"
#include "kernel/kdb.h"
#include "kernel/mm.h"
#include "kernel/vm.h"
#include "kernel-md/param.h"

namespace pool
{
    namespace
    {
        util::List<Pool> allPools;
    }

    Pool::Pool(const char* name, const size_t itemSize)
        : p_Mutex("poolmtx")
        , p_ItemSize(itemSize)
    {
        KASSERT(itemSize >= sizeof(Item), "item size too small");
        KASSERT(itemSize <= PAGE_SIZE, "cannot handle pool items larger than a page");
        KASSERT(strlen(name) < sizeof(p_Name), "name too long");
        strcpy(p_Name, name);

        allPools.push_back(*this);
    }

    Pool::~Pool()
    {
        allPools.remove(*this);
    }

    void* Pool::AllocateItem()
    {
        MutexGuard g(p_Mutex);

        while(true) {
            if (!p_AvailableItems.empty()) {
                auto& item = p_AvailableItems.front();
                p_AvailableItems.pop_front();
                return reinterpret_cast<void*>(&item);
            }

            // TODO: see if we are allowed to grow
            Grow();
        }
    }

    void Pool::FreeItem(void* ptr)
    {
        auto item = reinterpret_cast<Item*>(ptr);
        new (item) Item();

        MutexGuard g(p_Mutex);
        p_AvailableItems.push_back(*item);
    }

    void Pool::Grow()
    {
        Page* page{};
        auto ptr = page_alloc_single_mapped(page, vm::flag::Read | vm::flag::Write);
        KASSERT(page != nullptr, "deal with out of memory");
        p_AllocatedPages.push_back(*page);

        const auto itemsPerPage = PAGE_SIZE / p_ItemSize;
        auto item = static_cast<Item*>(ptr);
        for(int n = 0; n < itemsPerPage; ++n) {
            new (item) Item();
            p_AvailableItems.push_back(*item);
            item = reinterpret_cast<Item*>(reinterpret_cast<char*>(item) + p_ItemSize);
        }
    }

    const kdb::RegisterCommand kdbPool("pool", "Display memory pools", [](int, const kdb::Argument*) {
        auto countItems = [](auto& p) {
            size_t count{};
            for(auto& item: p) { ++count; }
            return count;
        };

        for(auto& pool: allPools) {
            kprintf("pool %p '%s': %d page(s) allocated, %d items available\n",
             &pool, pool.p_Name,
             countItems(pool.p_AllocatedPages),
             countItems(pool.p_AvailableItems));
        }
    });
}
