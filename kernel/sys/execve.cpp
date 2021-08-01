/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include <ananas/syscalls.h>
#include "kernel/exec.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vmspace.h"
#include "kernel-md/md.h"

struct Arguments {
    Arguments(const char** argp)
    {
        // Calculate the number of arguments and length
        int argc = 1 /* terminating nullptr */;
        size_t bytes_needed = 0;
        for (auto p = argp; *p != nullptr; p++) {
            bytes_needed += strlen(*p) + 1 /* terminating \0 */;
            argc++;
        }

        // Now allocate the data
        a_args = new char[bytes_needed + sizeof(char*) * argc];
        auto entry_ptr = reinterpret_cast<char**>(a_args);
        char* data_ptr = &a_args[sizeof(char*) * argc];
        for (auto p = argp; *p != nullptr; p++) {
            *entry_ptr++ = data_ptr;
            strcpy(data_ptr, *p);
            data_ptr += strlen(*p) + 1 /* terminating \0 */;
        }
        *entry_ptr = nullptr;
    }

    ~Arguments() { delete[] a_args; }

    const char** GetArgs() const { return reinterpret_cast<const char**>(a_args); }

    Arguments(const Arguments&) = delete;
    Arguments& operator=(const Arguments&) = delete;
  private:
    char* a_args;
};

Result sys_execve(const char* path, const char** argv, const char** envp)
{
    Process& proc = thread::GetCurrent().t_process;

    /* First step is to open the file */
    struct VFS_FILE file;
    if (auto result = vfs_open(&proc, path, proc.p_cwd, &file); result.IsFailure())
        return result;

    /*
     * Add a ref to the dentry; we'll be throwing away 'file' soon but we need to
     * keep the backing item intact.
     */
    DEntry& dentry = *file.f_dentry;
    dentry_ref(dentry);
    vfs_close(&proc, &file);

    // Prepare the executable; this verifies whether we have a shot at executing
    // this file.
    auto exec = exec_prepare(dentry);
    if (exec == nullptr) {
        dentry_deref(dentry);
        return Result::Failure(ENOEXEC);
    }

    // Prepare a copy of argv/envp - the next step will throw all mappings away
    Arguments copyArgv(argv);
    Arguments copyEnv(envp);

    // Grab the vmspace of the process and clean it; this should only leave the
    // kernel stack, which we're currently using - any other mappings are gone
    auto& vmspace = proc.p_vmspace;
    vmspace.PrepareForExecute();

    /*
     * Attempt to load the executable; if this fails, our vmspace will be in some
     * inconsistent state and we have no choice but to destroy the process.
     */
    void* auxargs;
    if (auto result = exec->Load(vmspace, dentry, auxargs); result.IsFailure()) {
        panic("TODO deal with exec load failures");
        dentry_deref(dentry);
        return result;
    }

    /*
     * Loading went okay; we can now set the new thread name (we won't be able to
     * access argv after the vmspace_clone() so best do it here)
     */
    auto& t = thread::GetCurrent();
    exec->PrepareForExecute(vmspace, t, auxargs, copyArgv.GetArgs(), copyEnv.GetArgs());
    dentry_deref(dentry);
    return Result::Success();
}
