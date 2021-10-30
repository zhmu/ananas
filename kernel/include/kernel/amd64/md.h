/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"

class Result;
struct Thread;
struct VMSpace;

namespace md
{
    namespace thread
    {
        typedef void (*kthread_func_t)(void*);

        Result InitUserlandThread(Thread& thread, int flags);
        void InitKernelThread(Thread& thread, kthread_func_t func, void* arg);
        void Free(Thread& thread);

        Thread& SwitchTo(Thread& new_thread, Thread& old_thread);

        void* Map(Thread& thread, void* to, void* from, size_t length, int flags);
        Result Unmap(Thread& t, addr_t virt, size_t len);
        void* MapThreadMemory(Thread& thread, void* ptr, size_t length, int write);
        void Clone(Thread& t, Thread& parent, register_t retval);
        Result Unmap(Thread& thread, addr_t virt, size_t length);
        void SetupPostExec(Thread& thread, addr_t exec_addr, addr_t stack_addr);

    } // namespace thread

    namespace vm
    {
        // Maps relevant kernel addresses into the vmspace
        void MapKernelSpace(VMSpace& vs);

        /* Maps a piece of memory for kernel use */
        void MapKernel(addr_t phys, addr_t virt, size_t num_pages, int flags);

        /* Unmaps a piece of kernel memory */
        void UnmapKernel(addr_t virt, size_t num_pages);

        // Maps 'num_pages' at physical address 'phys' to virtual address 'virt' for vmspace 'vs'
        // with flags 'flags'
        void MapPages(VMSpace& vs, addr_t virt, addr_t phys, size_t num_pages, int flags);

        // Unmaps 'num_pages' at virtual address virt for vmspace 'vs'
        void UnmapPages(VMSpace& vs, addr_t virt, size_t num_pages);

    } // namespace vm

    namespace vmspace
    {
        Result Init(VMSpace& vs);
        void Destroy(VMSpace& vs);

    } // namespace vmspace

    void PowerDown();
    void Reboot();

} // namespace md
