/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <ananas/types.h>

struct SYSCALL_ARGS {
    register_t number;
    register_t arg1, arg2, arg3, arg4, arg5;
};

class Result;
struct FD;
struct VFS_FILE;

register_t syscall(struct SYSCALL_ARGS* args);

Result syscall_get_fd(int type, fdindex_t index, FD*& fd_out);
Result syscall_get_file(fdindex_t index, struct VFS_FILE** out);
Result syscall_map_string(const void* ptr, const char** out);
Result syscall_map_buffer(const void* ptr, size_t len, int flags, void** out);
Result syscall_fetch_offset(const void* ptr, off_t* out);
Result syscall_set_offset(void* ptr, off_t len);

#endif /* __SYSCALL_H__ */
