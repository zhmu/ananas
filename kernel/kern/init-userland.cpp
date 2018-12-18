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
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/mount.h"
#include "kernel-md/md.h"
#include "options.h"

namespace
{
    void userinit_func(void*)
    {
        /* We expect root=type:device here */
        const char* rootfs_arg = cmdline_get_string("root");
        if (rootfs_arg == NULL) {
            kprintf("'root' option not specified, not mounting a root filesystem\n");
            thread_exit(0);
        }

        char rootfs_type[64];
        const char* p = strchr(rootfs_arg, ':');
        if (p == NULL) {
            kprintf("cannot parse 'root' - expected type:device\n");
            thread_exit(0);
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

#ifdef FS_ANKHFS
        kprintf("- Mounting /ankh...");
        if (auto result = vfs_mount(nullptr, "/ankh", "ankhfs"); result.IsSuccess())
            kprintf(" success\n");
        else
            kprintf(" error %d\n", result.AsStatusCode());
#endif

        // Now it makes sense to try to load init
        const char* init_path = cmdline_get_string("init");
        if (init_path == nullptr)
            init_path = "/sbin/init";

        Process* proc;
        if (auto result = process_alloc(nullptr, proc); result.IsFailure()) {
            kprintf("couldn't create process, %i\n", result.AsStatusCode());
            thread_exit(0);
        }
        // Note: proc is locked at this point and has a single ref
        proc->p_parent = proc; // XXX init is its own parent

        Thread* t;
        {
            Result result = thread_alloc(*proc, t, "init", THREAD_ALLOC_DEFAULT);
            proc->Unlock();
            if (result.IsFailure()) {
                kprintf("couldn't create thread, %i\n", result.AsStatusCode());
                thread_exit(0);
            }
        }

        kprintf("- Lauching init from %s...", init_path);
        struct VFS_FILE file;
        if (auto result = vfs_open(proc, init_path, proc->p_cwd, &file); result.IsFailure()) {
            kprintf("couldn't open init executable, %i\n", result.AsStatusCode());
            thread_exit(0);
        }
        DEntry& dentry = *file.f_dentry;
        dentry_ref(dentry);
        vfs_close(proc, &file);

        auto exec = exec_prepare(dentry);
        if (exec != nullptr) {
            void* auxargs;
            auto& vmspace = *proc->p_vmspace;
            if (auto result = exec->Load(vmspace, dentry, auxargs); result.IsSuccess()) {
                kprintf(" ok\n");
                const char* argv[] = {"init", nullptr};
                const char* envp[] = {"OS=Ananas", "USER=root", nullptr};
                exec->PrepareForExecute(vmspace, *t, auxargs, argv, envp);
                t->Resume();
            } else {
                kprintf(" fail - error %i\n", result.AsStatusCode());
            }
        } else {
            kprintf(" fail - not an executable\n");
        }

        dentry_deref(dentry);
        thread_exit(0);
    }

    Thread userinit_thread;

    const init::OnInit initUserlandInit(init::SubSystem::Scheduler, init::Order::Last, []() {
        kthread_init(userinit_thread, "user-init", &userinit_func, nullptr);
        userinit_thread.Resume();
    });

} // unnamed namespace
