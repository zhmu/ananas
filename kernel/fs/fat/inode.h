/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"

class Result;
struct INode;

Result fat_prepare_inode(INode& inode);
void fat_discard_inode(INode& inode);
Result fat_read_inode(INode& inode, ino_t inum);
Result fat_write_inode(INode& inode);

extern struct VFS_INODE_OPS fat_inode_ops;
