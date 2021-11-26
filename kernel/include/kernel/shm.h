/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"
#include <ananas/util/list.h>
#include <ananas/util/vector.h>
#include "kernel/lock.h"
#include "kernel/lib.h"
#include "kernel/refcount.h"
#include "kernel/result.h"

struct Process;
struct VMPage;
struct VMArea;

namespace shm
{
    // Details a shared memory object which can be shared between multiple
    // processes and thus is refcounted
    struct SharedMemory final : util::List<SharedMemory>::NodePtr
    {
        refcount_t shm_refcount{1};
        const int shm_id;
        util::vector<VMPage*> shm_pages;
        bool destroyed{};

        SharedMemory(size_t);
        void Ref();
        void Deref();

    private:
        ~SharedMemory();
    };

    struct ProcessSpecificData final
    {
        // Binds the shared memory object to a process' virtual address space
        struct SharedMemoryMapping
        {
            addr_t virt;
            SharedMemory* sm;
            VMArea* va;

            friend bool operator==(const SharedMemoryMapping& a, const SharedMemoryMapping& b)
            {
                return a.virt == b.virt && a.sm == b.sm && a.va == b.va;
            }

            friend bool operator!=(const SharedMemoryMapping& a, const SharedMemoryMapping& b)
            {
                return !(a == b);
            }
        };

        util::vector<SharedMemoryMapping> shm_mappings;
    };

    SharedMemory* Allocate(size_t size);
    SharedMemory* FindById(int id);
    Result Map(SharedMemory& sm, void*& ptr);
    Result Unmap(const void* ptr);

    void OnProcessDestroy(Process& p);
}
