/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/cmdline.h"
#include "kernel/exec.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/time.h"
#include "kernel/userland.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/mount.h"
#include "kernel-md/md.h"

#include "kernel-md/param.h"
#include "kernel/vmspace.h"
#include "kernel/vmarea.h"
#include "kernel/vm.h"

extern "C" void* initcode;
extern "C" void* initcode_end;

namespace
{
    Thread* userinit_thread{};

    const char* get_cmdline_or_default(const char* key, const char* dfault)
    {
        const auto result = cmdline_get_string(key);
        if (result == nullptr) return dfault;
        return result;
    }

    addr_t InitStack(void* p, addr_t stackEnd)
    {
        const auto init_arg = get_cmdline_or_default("init", "/sbin/init");
        const auto rootfs_arg = get_cmdline_or_default("root", "");
        const auto data_bytes_needed = strlen(init_arg) + 1 + strlen(rootfs_arg) + 1;
        const auto record_bytes_needed = sizeof(register_t) * 2;
        const size_t padding_bytes_needed = userland::CalculatePaddingBytesNeeded(data_bytes_needed);
        const size_t bytes_needed = record_bytes_needed + data_bytes_needed + padding_bytes_needed;
        KASSERT(bytes_needed < PAGE_SIZE, "TODO deal with more >1 page here!");

        auto stack_ptr = reinterpret_cast<addr_t>(p) + PAGE_SIZE - bytes_needed;
        auto stack_data_ptr = stackEnd - data_bytes_needed - padding_bytes_needed;

        using userland::Store;
        Store(stack_ptr, stack_data_ptr); /* &init_arg */
        stack_data_ptr += strlen(init_arg) + 1 /* terminating \0 */;
        Store(stack_ptr, stack_data_ptr); /* &rootfs_arg */
        stack_data_ptr += strlen(rootfs_arg) + 1 /* terminating \0 */;

        Store(stack_ptr, *init_arg, strlen(init_arg) + 1 /* terminating \0 */);
        Store(stack_ptr, *rootfs_arg, strlen(rootfs_arg) + 1 /* terminating \0 */);
        Store(stack_ptr, static_cast<register_t>(0), padding_bytes_needed); // padding
        KASSERT(
            stack_ptr == reinterpret_cast<addr_t>(p) + PAGE_SIZE,
            "did not fill entire userland stack; ptr %p != end %p\n", stack_ptr,
            reinterpret_cast<addr_t>(p) + PAGE_SIZE);
        return stackEnd - bytes_needed;
    }

    void FillInitThread(Thread& t)
    {
        auto& vs = t.t_process.p_vmspace;
        const auto stackEnd = USERLAND_STACK_ADDR + THREAD_STACK_SIZE;
        constexpr addr_t userlandCodeAddr = 0x100'000;

        // Create stack
        const auto stack_ptr = [&] {
            auto& stack_vp = userland::CreateStack(vs, stackEnd);
            auto p = userland::MapPageToKernel(stack_vp);
            auto stack_ptr = InitStack(p, stackEnd);
            userland::UnmapPage(p);
            stack_vp.Unlock();
            return stack_ptr;
        }();

        // Create code
        {
            auto& code_vp = userland::CreateCode(vs, userlandCodeAddr, PAGE_SIZE);
            auto p = userland::MapPageToKernel(code_vp);
            memcpy(p, &initcode, (addr_t)&initcode_end - (addr_t)&initcode);
            userland::UnmapPage(p);
            code_vp.Unlock();
        }

        t.SetName("init");
        md::thread::SetupPostExec(t, userlandCodeAddr, stack_ptr);
    }

    void userinit_func(void*)
    {
        auto& thread = *userinit_thread;

        /* We expect root=type:device here */
        const char* rootfs_arg = cmdline_get_string("root");
        if (rootfs_arg == NULL) {
            kprintf("'root' option not specified, not mounting a root filesystem\n");
            thread.Terminate(0);
        }

        char rootfs_type[64];
        const char* p = strchr(rootfs_arg, ':');
        if (p == NULL) {
            kprintf("cannot parse 'root' - expected type:device\n");
            thread.Terminate(0);
        }

        memcpy(rootfs_type, rootfs_arg, p - rootfs_arg);
        rootfs_type[p - rootfs_arg] = '\0';
        const char* rootfs = p + 1;

        kprintf("- Mounting / (type %s) from %s...", rootfs_type, rootfs);
        while (true) {
            if (auto result = vfs_mount(rootfs, "/", rootfs_type); result.IsSuccess())
                break;
            else
                kprintf(" failure %d, retrying...", result.AsStatusCode());

            thread_sleep_ms(1000); // 1 second
        }
        kprintf(" ok\n");

        Process* proc;
        if (auto result = process_alloc(nullptr, proc); result.IsFailure()) {
            kprintf("couldn't create process, %i\n", result.AsStatusCode());
            thread.Terminate(0);
        }
        // Note: proc is locked at this point and has a single ref
        proc->p_parent = proc; // XXX init is its own parent

        Thread* t;
        {
            Result result = thread_alloc(*proc, t, "init", THREAD_ALLOC_DEFAULT);
            proc->Unlock();
            if (result.IsFailure()) {
                kprintf("couldn't create thread, %i\n", result.AsStatusCode());
                thread.Terminate(0);
            }
        }

        FillInitThread(*t);
        t->Resume();
        thread.Terminate(0);
    }

    const init::OnInit initUserlandInit(init::SubSystem::Scheduler, init::Order::Last, []() {
        if (auto result = kthread_alloc("user-init", &userinit_func, NULL, userinit_thread);
            result.IsFailure())
            panic("cannot create userinit thread");
        userinit_thread->Resume();
    });

} // unnamed namespace
