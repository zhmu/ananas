/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"

struct INode;
class Result;

Result fat_block_map(INode& inode, blocknr_t block_in, blocknr_t& block_out, bool create);
void fat_dump_cache(struct VFS_MOUNTED_FS* fs);
int fat_clear_cache(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster);
Result fat_truncate_clusterchain(INode& inode);
Result fat_update_infosector(struct VFS_MOUNTED_FS* fs);

extern struct VFS_INODE_OPS fat_inode_ops;
