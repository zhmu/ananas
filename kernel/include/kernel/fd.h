/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/util/list.h>
#include <ananas/_types/socklen.h>
#include <sys/select.h>
#include "kernel/lock.h"
#include "kernel/init.h"
#include "kernel/vfs/types.h"

#define FD_TYPE_ANY -1
#define FD_TYPE_UNUSED 0
#define FD_TYPE_FILE 1
#define FD_TYPE_SOCKET 2

struct Process;
struct FDOperations;
class Result;

namespace net { struct LocalSocket; }

struct FD : util::List<FD>::NodePtr {
    int fd_type = 0;                      /* one of FD_TYPE_... */
    int fd_flags = 0;                     /* flags */
    Process* fd_process = nullptr;        /* owning process */
    Mutex fd_mutex{"fd"};                 /* mutex guarding the descriptor */
    const FDOperations* fd_ops = nullptr; /* descriptor operations */

    // Descriptor-specific data
    union {
        struct VFS_FILE d_vfs_file;
        net::LocalSocket* d_local_socket;
    } fd_data{};

    Result Close();
};

// Descriptor operations map almost directly to the syscalls invoked on them.
struct CLONE_OPTIONS;
typedef Result (*fd_read_fn)(fdindex_t index, FD& fd, void* buf, size_t len);
typedef Result (*fd_write_fn)(fdindex_t index, FD& fd, const void* buf, size_t len);
typedef Result (*fd_open_fn)(fdindex_t index, FD& fd, const char* path, int flags, int mode);
typedef Result (*fd_free_fn)(Process& proc, FD& fd);
typedef Result (*fd_unlink_fn)(fdindex_t index, FD& fd);
typedef Result (*fd_clone_fn)(
    Process& proc_in, fdindex_t index, FD& fd_in, struct CLONE_OPTIONS* opts, Process& proc_out,
    FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out);
typedef Result (*fd_ioctl_fn)(
    fdindex_t index, FD& fd, unsigned long request, void* args[]);

using fd_accept_fn = Result (*)(fdindex_t, FD& fd, struct sockaddr*, socklen_t*);
using fd_bind_fn = Result (*)(fdindex_t, FD& fd, const struct sockaddr*, socklen_t);
using fd_connect_fn = Result (*)(fdindex_t, FD& fd, const struct sockaddr*, socklen_t);
using fd_listen_fn = Result (*)(fdindex_t, FD& fd, int);
using fd_send_fn = Result (*)(fdindex_t, FD& fd, const void*, size_t, int);
using fd_can_read_fn = bool (*)(fdindex_t, FD&);
using fd_can_write_fn = bool (*)(fdindex_t, FD&);
using fd_has_except_fn = bool (*)(fdindex_t, FD&);

struct FDOperations {
    fd_read_fn d_read;
    fd_write_fn d_write;
    fd_open_fn d_open;
    fd_free_fn d_free;
    fd_unlink_fn d_unlink;
    fd_clone_fn d_clone;
    fd_ioctl_fn d_ioctl;
    // Socket-related
    fd_accept_fn d_accept;
    fd_bind_fn d_bind;
    fd_connect_fn d_connect;
    fd_listen_fn d_listen;
    fd_send_fn d_send;
    fd_can_read_fn d_can_read;
    fd_can_write_fn d_can_write;
    fd_has_except_fn d_has_except;
};

/* Registration of descriptor types */
struct FDType : util::List<FDType>::NodePtr {
    FDType(const char* name, int id, FDOperations& fdops) : ft_name(name), ft_id(id), ft_ops(fdops)
    {
    }
    const char* ft_name;
    const int ft_id;
    const FDOperations& ft_ops;
};

namespace fd
{
    void Initialize();
    Result
    Allocate(int type, Process& proc, fdindex_t index_from, FD*& fd_out, fdindex_t& index_out);
    Result Lookup(Process& proc, fdindex_t index, int type, FD*& fd_out);
    Result FreeByIndex(Process& proc, fdindex_t index);
    Result Clone(
        Process& proc_in, fdindex_t index_in, struct CLONE_OPTIONS* opts, Process& proc_out,
        FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out);

    // Only to be used from descriptor implementation code
    Result CloneGeneric(
        FD& fd_in, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out);
    void RegisterType(FDType& ft);
    void UnregisterType(FDType& ft);

} // namespace fd
