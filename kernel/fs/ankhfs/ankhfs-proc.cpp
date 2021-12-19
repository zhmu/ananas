/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/errno.h>
#include "kernel/lib.h"
#include "kernel/fd.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "kernel/vm.h"
#include "kernel/vmarea.h"
#include "kernel/vmspace.h"
#include "proc.h"
#include "support.h"

namespace process
{
    extern Mutex process_mtx;
    extern process::ProcessList process_all;

} // namespace process

namespace ankhfs
{
    namespace
    {
        constexpr unsigned int subName = 1;
        constexpr unsigned int subVmSpace = 2;
        constexpr unsigned int subStatus = 3;
        constexpr unsigned int subFdDir = 4;
        constexpr unsigned int subFdEntry = 5;

        struct DirectoryEntry proc_entries[] = {{"name", make_inum(SS_Proc, 0, subName)},
                                                {"vmspace", make_inum(SS_Proc, 0, subVmSpace)},
                                                {"status", make_inum(SS_Proc, 0, subStatus)},
                                                {"fd", make_inum(SS_Proc, 0, subFdDir)},
                                                {NULL, 0}};

        Result HandleReadDir_Proc_Root(struct VFS_FILE* file, void* dirents, size_t len)
        {
            struct FetchEntry : IReadDirCallback {
                bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) override
                {
                    if (currentProcess == process::process_all.end())
                        return false;

                    // XXX we should lock currentProcess here
                    snprintf(entry, maxLength, "%d", static_cast<int>(currentProcess->p_pid));
                    inum = make_inum(SS_Proc, currentProcess->p_pid, 0);
                    ++currentProcess;
                    return true;
                }

                process::ProcessList::iterator currentProcess = process::process_all.begin();
            };

            // Fill the root directory with one directory per process ID
            MutexGuard g(process::process_mtx);
            FetchEntry entryFetcher;
            return HandleReadDir(file, dirents, len, entryFetcher);
        }

        Result HandleReadDir_Proc_Fd(struct VFS_FILE* file, void* dirents, size_t len)
        {
            struct FetchFdEntry : IReadDirCallback {
                FetchFdEntry(Process& process) : currentProcess(process) {}

                bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) override
                {
                    while (currentFd < PROCESS_MAX_DESCRIPTORS) {
                        auto& fd = currentProcess.p_fd[currentFd];
                        if (fd != nullptr) {
                            snprintf(entry, maxLength, "%d", static_cast<int>(currentFd));
                            inum = make_inum(SS_Proc, currentProcess.p_pid, subFdEntry + currentFd);
                            ++currentFd;
                            return true;
                        }
                        ++currentFd;
                    }
                    return false;
                }

                int currentFd = 0;
                Process& currentProcess;
            };

            // Fetch the process
            ino_t inum = file->f_dentry->d_inode->i_inum;
            pid_t pid = static_cast<pid_t>(inum_to_id(inum));
            Process* p = process_lookup_by_id_and_lock(pid);
            if (p == nullptr)
                return Result::Failure(EIO);

            // Fill the directory with one item per active file descriptor
            FetchFdEntry entryFetcher(*p);
            auto result = HandleReadDir(file, dirents, len, entryFetcher);
            p->Unlock();
            return result;
        }

        class ProcSubSystem : public IAnkhSubSystem
        {
          public:
            Result HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len) override
            {
                INode& inode = *file->f_dentry->d_inode;
                ino_t inum = inode.i_inum;

                if (inum_to_id(inum) == 0)
                    return HandleReadDir_Proc_Root(file, dirents, len);

                if (inum_to_sub(inum) == subFdDir)
                    return HandleReadDir_Proc_Fd(file, dirents, len);

                return ankhfs::HandleReadDir(file, dirents, len, proc_entries[0], inum_to_id(inum));
            }

            Result FillInode(INode& inode, ino_t inum) override
            {
                auto sub = inum_to_sub(inum);
                if (sub == 0 || sub == subFdDir) {
                    inode.i_sb.st_mode |= S_IFDIR;
                } else if (sub >= subFdEntry && sub <= subFdEntry + PROCESS_MAX_DESCRIPTORS) {
                    inode.i_sb.st_mode |= S_IFLNK;
                } else {
                    inode.i_sb.st_mode |= S_IFREG;
                }
                return Result::Success();
            }

            Result HandleReadLink(INode& inode, void* buf, size_t len) override
            {
                auto sub = inum_to_sub(inode.i_inum);
                if (sub >= subFdEntry && sub <= subFdEntry + PROCESS_MAX_DESCRIPTORS) {
                    auto pid = static_cast<pid_t>(inum_to_id(inode.i_inum));
                    Process* p = process_lookup_by_id_and_lock(pid);
                    if (p == nullptr)
                        return Result::Failure(EIO);

                    Result result = Result::Failure(EIO);
                    auto fd = p->p_fd[sub - subFdEntry]; // XXX we need to lock this
                    if (fd != nullptr && fd->fd_type == FD_TYPE_FILE &&
                        fd->fd_data.d_vfs_file.f_dentry != nullptr) {
                        auto len_needed = dentry_construct_path(
                            static_cast<char*>(buf), len, *fd->fd_data.d_vfs_file.f_dentry);
                        if (len_needed < len)
                            len = len_needed;
                        result = Result::Success(len);
                    }
                    p->Unlock();
                    return result;
                }

                return Result::Failure(EIO);
            }

            Result HandleRead(struct VFS_FILE* file, void* buf, size_t len) override
            {
                ino_t inum = file->f_dentry->d_inode->i_inum;

                pid_t pid = static_cast<pid_t>(inum_to_id(inum));
                Process* p = process_lookup_by_id_and_lock(pid);
                if (p == nullptr)
                    return Result::Failure(EIO);

                char result[256]; // XXX
                strcpy(result, "???");
                switch (inum_to_sub(inum)) {
                    case subName: {
                        if (p->p_mainthread != nullptr)
                            strncpy(result, p->p_mainthread->t_name, sizeof(result));
                        break;
                    }
                    case subVmSpace: {
                        // XXX shouldn't we lock something here?'
                        char* r = result;
                        for (const auto& [ interval, va ] : p->p_vmspace.vs_areamap) {
                            snprintf(
                                r, sizeof(result) - (r - result), "%p %p %c%c%c\n",
                                reinterpret_cast<void*>(va->va_virt),
                                reinterpret_cast<void*>(va->va_len),
                                (va->va_flags & vm::flag::Read) ? 'r' : '-',
                                (va->va_flags & vm::flag::Write) ? 'w' : '-',
                                (va->va_flags & vm::flag::Execute) ? 'x' : '-');
                            r += strlen(r);
                        }
                        break;
                    }
                    case subStatus: {
                        // No need to lock the parent, the child should have Ref'ed it
                        // <state> <pid> <utime> <stime> <cutime> <cstime>
                        snprintf(
                            result, sizeof(result), "%d %d %d %d %d %d\n",
                            p->p_mainthread ? static_cast<int>(p->p_mainthread->t_state) : -1,
                            p->p_parent != nullptr ? p->p_parent->p_pid : 0,
                            p->p_times.tms_utime, p->p_times.tms_stime,
                            p->p_times.tms_cutime, p->p_times.tms_cstime);
                        break;
                    }
                }
                result[sizeof(result) - 1] = '\0';
                p->Unlock();
                return ankhfs::HandleRead(file, buf, len, result);
            }

            Result HandleIOControl(struct VFS_FILE* file, unsigned long op, void* args[]) override
            {
                return Result::Failure(EIO);
            }

            Result HandleOpen(VFS_FILE& file, Process* p) override { return Result::Success(); }

            Result HandleClose(VFS_FILE& file, Process* p) override { return Result::Success(); }
        };

    } // unnamed namespace

    IAnkhSubSystem& GetProcSubSystem()
    {
        static ProcSubSystem procSubSystem;
        return procSubSystem;
    }

} // namespace ankhfs
