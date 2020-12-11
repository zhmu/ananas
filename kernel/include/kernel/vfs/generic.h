/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

struct DEntry;
struct INode;
class Result;

Result vfs_generic_lookup(DEntry& dirinode, INode*& destinode, const char* dentry);
Result vfs_generic_read(struct VFS_FILE* file, void* buf, size_t len);
Result vfs_generic_write(struct VFS_FILE* file, const void* buf, size_t len);
Result vfs_generic_follow_link(INode& inode, DEntry& base, DEntry*& result);
