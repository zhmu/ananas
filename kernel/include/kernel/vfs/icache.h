/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_ICACHE_H__
#define __ANANAS_ICACHE_H__

#include <ananas/types.h>
#include "kernel/list.h"

struct INode;
struct VFS_MOUNTED_FS;

void icache_remove_inode(INode& inode);
/*
 * Removes an inode reference; cleans up the inode if the refcount is zero.
 */
void vfs_deref_inode(INode& inode);

/*
 * Explicitely add a reference to a given inode.
 */
void vfs_ref_inode(INode& inode);

/* Low-level interface - designed for filesystem use (must not be used otherwise) */

/* Marks an inode as dirty; will trigger the filesystem's 'write_inode' function */
void vfs_set_inode_dirty(INode& inode);

/* Internal interface only */
void vfs_dump_inode(INode& inode);

#endif /* __ANANAS_ICACHE_H__ */
