/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <sys/shm.h>
#include "kernel/lib.h"
#include "kernel/shm.h"
#include "kernel/process.h"
#include "kernel/vm.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "kernel-md/param.h"

namespace shm {
    namespace {
        Mutex mtx_shm{"allShm"};
        int next_shm_id{1};
        util::List<SharedMemory> allSharedMemory;

        auto AllocateId()
        {
            MutexGuard g(mtx_shm);
            return next_shm_id++;
        }
    }

    SharedMemory::SharedMemory(const size_t size)
        : shm_id(AllocateId())
    {
        KASSERT((size & (PAGE_SIZE - 1)) == 0, "size not a multiple of PAGE_SIZE");
        const auto numPages = size / PAGE_SIZE;
        shm_pages.resize(numPages);
        for (size_t n = 0; n < numPages; ++n) {
            shm_pages[n] = &vmpage::Allocate(vmpage::flag::Promoted);
            shm_pages[n]->Unlock();
        }

        MutexGuard g(mtx_shm);
        allSharedMemory.push_back(*this);
    }

    SharedMemory::~SharedMemory()
    {
        KASSERT(shm_refcount == 0, "shm still has %d refs", shm_refcount);
        for(auto page: shm_pages) {
            page->Lock();
            page->Deref();
        }

        MutexGuard g(mtx_shm);
        allSharedMemory.remove(*this);
    }

    void SharedMemory::Ref()
    {
        ++shm_refcount;
    }

    void SharedMemory::Deref()
    {
        if (--shm_refcount > 0) return;
        delete this;
    }

    SharedMemory* Allocate(size_t size)
    {
        return new SharedMemory(size);
    }

    SharedMemory* FindById(int id)
    {
        MutexGuard g(mtx_shm);
        for(auto& shm: allSharedMemory) {
            if (shm.shm_id == id) return &shm;
        }
        return nullptr;
    }

    Result Map(SharedMemory& sm, void*& ptr)
    {
        auto& proc = process::GetCurrent();
        auto& vs = proc.p_vmspace;
        const auto length = sm.shm_pages.size() * PAGE_SIZE;
        const auto virt = vs.ReserveAdressRange(length);
        int flags = vm::flag::Private | vm::flag::User | vm::flag::Read | vm::flag::Write;
        VMArea* va;
        vs.Map({ virt, virt + length }, 0, flags, flags, va);

        kprintf("shm %p id %d pid %d: mapping to %p\n", &sm, sm.shm_id, proc.p_pid, virt);

        addr_t mapVirt = virt;
        for(size_t n = 0; n < sm.shm_pages.size(); ++n) {
            auto& p = *sm.shm_pages[n];
            p.Lock();
            p.Ref();
            p.Map(vs, *va, mapVirt);
            p.Unlock();
            mapVirt += PAGE_SIZE;
        }

        {
            MutexGuard g(mtx_shm);
            proc.p_shm.shm_mappings.push_back({ virt, &sm, va });
        }

        ptr = reinterpret_cast<void*>(virt);
        return Result::Success();
    }

    Result Unmap(const void* ptr)
    {
        auto& proc = process::GetCurrent();
        auto& vs = proc.p_vmspace;

        MutexGuard g(mtx_shm);
        for(auto& smm: proc.p_shm.shm_mappings) {
            if (smm.virt == reinterpret_cast<addr_t>(ptr)) {
                auto& sm = *smm.sm;
                for(auto p: sm.shm_pages) {
                    p->Lock();
                    p->Deref();
                }

                sm.Deref();
                vs.FreeArea(*smm.va);
                proc.p_shm.shm_mappings.remove(smm);
                return Result::Success();
            }
        }
        return Result::Success(EINVAL);
    }

    void OnProcessDestroy(Process& p)
    {
        // No need to unmap as things are about to be destroyed anyway
        for(auto& smm: p.p_shm.shm_mappings) {
            smm.sm->Deref();
        }
        p.p_shm.shm_mappings.clear();
    }
}
