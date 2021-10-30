/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"

struct INode;

namespace ankhfs
{
    enum SubSystem {
        SS_Root,
        SS_Proc,
        SS_FileSystem,
        SS_Device,
        SS_Kernel,
        SS_Network,
        SS_Last // do not use
    };

    struct DirectoryEntry {
        const char* de_name;
        ino_t de_inum;
    };

    // Inode numbers are [type] [id] [sub] for us

    constexpr ino_t make_inum(SubSystem subsystem, unsigned int id, unsigned int sub)
    {
        return static_cast<ino_t>(subsystem) << 40UL | id << 8UL | sub;
    }

    constexpr unsigned int inum_to_id(ino_t ino) { return (ino >> 8) & 0xffffffff; }

    constexpr unsigned int inum_to_sub(ino_t ino) { return ino & 0xff; }

    constexpr SubSystem inum_to_subsystem(ino_t ino) { return static_cast<SubSystem>(ino >> 40); }

    class IReadDirCallback
    {
      public:
        virtual bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) = 0;
    };

    class IAnkhSubSystem
    {
      public:
        virtual Result HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len) = 0;
        virtual Result HandleRead(struct VFS_FILE* file, void* buf, size_t len) = 0;
        virtual Result HandleIOControl(struct VFS_FILE* file, unsigned long op, void* args[]) = 0;
        virtual Result HandleReadLink(INode& inode, void* buf, size_t len) = 0;
        virtual Result HandleOpen(VFS_FILE& file, Process* p) = 0;
        virtual Result HandleClose(VFS_FILE& file, Process* p) = 0;
        virtual Result FillInode(INode& inode, ino_t inum) = 0;
    };

    Result
    HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len, IReadDirCallback& callback);
    Result HandleReadDir(
        struct VFS_FILE* file, void* dirents, size_t len, const DirectoryEntry& firstEntry,
        unsigned int id = 0);
    Result HandleRead(struct VFS_FILE* file, void* buf, size_t len, const char* data);

} // namespace ankhfs
