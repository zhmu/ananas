/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include <ananas/errno.h>
#include "kernel/bio.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/result.h"
#include "kernel/schedule.h" // XXX
#include "kernel/vfs/types.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/icache.h"
#include "block.h"
#include "fat.h"
#include "fatfs.h"

static inline blocknr_t fat_cluster_to_sector(struct VFS_MOUNTED_FS* fs, uint32_t cluster)
{
    auto privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
    KASSERT(cluster >= 2, "invalid cluster number %u specified", cluster);

    blocknr_t ret = cluster - 2 /* really; the first 2 are reserved or something? */;
    ret *= privdata->sectors_per_cluster;
    ret += privdata->first_data_sector;
    return ret;
}

static inline void fat_make_cluster_block_offset(
    struct VFS_MOUNTED_FS* fs, uint32_t cluster, blocknr_t* sector, uint32_t* offset)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    uint32_t block;
    switch (fs_privdata->fat_type) {
        case 16:
            block = 2 * cluster;
            break;
        case 32:
            block = 4 * cluster;
            break;
        default:
            panic("fat_get_cluster(): unsupported fat size %u", fs_privdata->fat_type);
    }
    *sector = fs_privdata->reserved_sectors + (block / fs_privdata->sector_size);
    *offset = block % fs_privdata->sector_size;
}

/*
 * Used to obtain the clusternum'th cluster of a file starting at
 * first_cluster. Returns BAD_RANGE error if end-of-file was found (but
 * cluster_out will be set to the final cluster found_
 */
static Result fat_get_cluster(
    struct VFS_MOUNTED_FS* fs, uint32_t first_cluster, uint32_t clusternum, uint32_t* cluster_out)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
    uint32_t cur_cluster = first_cluster;

    /*
     * First of all, look through our cache. XXX linear search.
     *
     * We assume the cache items will never be removed while we are working with
     * the file because it will remain referenced - we throw away the cache items
     * when the inode is freed.
     *
     * Note that we explicitely _do not_ cache cluster -1; it is used to find the
     * final cluster of the FAT chain.
     */
try_cache:; /* dummy ; to keep gcc happy */
    int create = 0, found = 0;
    struct FAT_CLUSTER_CACHEITEM* ci = NULL;
    struct FAT_CLUSTER_CACHEITEM* ci_closest = NULL;
    if (clusternum != -1) {
        {
            SpinlockGuard g(fs_privdata->spl_cache);
            for (int cache_item = 0; cache_item < FAT_NUM_CACHEITEMS; cache_item++) {
                ci = &fs_privdata->cluster_cache[cache_item];
                if (ci->f_clusterno == 0) {
                    /* Found empty item - use it (we assume this always occurs at the end of the
                     * items) */
                    create++;
                    ci->f_clusterno = first_cluster;
                    ci->f_index = clusternum;
                    ci->f_nextcluster = 0;
                    break;
                }
                if (ci->f_clusterno == first_cluster) {
                    if (ci->f_index == clusternum) {
                        /* Got it */
                        found++;
                        break;
                    }

                    /*
                     * Cluster is fine, but index isn't. Attempt to use this item as a base;
                     * the less items we'll have to walk through, the better.
                     */
                    if (ci->f_index < clusternum &&
                        (ci_closest == NULL || ci->f_index > ci_closest->f_index))
                        ci_closest = ci;
                }
            }
            /*
             * If we need to do a lookup, use the closest item as base; we do this
             * while holding the lock to ensure the item won't vanish.
             */
            if (create && !found && ci_closest != NULL) {
                cur_cluster = ci_closest->f_nextcluster;
                clusternum -= ci_closest->f_index;
            }
        }

        if (create == 0 && found == 0) {
            /*
             * Out of cache items; we'll just throw away all cached clusters of this
             * item. If this doesn't work, we'll just overwrite the first item XXX
             */
            if (fat_clear_cache(fs, first_cluster))
                goto try_cache;
            ci = &fs_privdata->cluster_cache[0];
            ci->f_clusterno = first_cluster;
            ci->f_index = clusternum;
            ci->f_nextcluster = 0;
            create++;
            kprintf("fat_get_cluster(): sacrificed first cache block\n");
        }
    }

    /*
     * If we are not creating, yet the next cluster value is zero, someone else is
     * looking up the item, we need to spin.
     */
    if (ci != NULL && create == 0) {
        while (ci->f_nextcluster == 0) {
            kprintf("fat_get_cluster(): pending cluster found, waiting...\n");
            scheduler::Schedule();
        }
        if (ci->f_nextcluster == -1)
            return Result::Failure(ERANGE);
        *cluster_out = ci->f_nextcluster;
        return Result::Success();
    }

    /* Not in the cache; we'll need to traverse the disk */
    *cluster_out = first_cluster;
    while (cur_cluster != 0 && clusternum-- > 0) {
        blocknr_t sector_num;
        uint32_t offset;
        fat_make_cluster_block_offset(fs, cur_cluster, &sector_num, &offset);
        BIO* bio;
        if (auto result = vfs_bread(fs, sector_num, &bio); result.IsFailure())
            return result;

        /* Grab the value from the FAT */
        switch (fs_privdata->fat_type) {
            case 16:
                cur_cluster = FAT_FROM_LE16((char*)(static_cast<char*>(bio->Data()) + offset));
                if (cur_cluster >= 0xfff8)
                    cur_cluster = 0;
                break;
            case 32: /* actually FAT-28... */
                cur_cluster =
                    FAT_FROM_LE32((char*)(static_cast<char*>(bio->Data()) + offset)) & 0xfffffff;
                if (cur_cluster >= 0xffffff8)
                    cur_cluster = 0;
                break;
        }
        bio->Release();
        if (cur_cluster != 0)
            *cluster_out = cur_cluster;
    }
    if (cur_cluster != 0) {
        if (ci != NULL)
            ci->f_nextcluster = *cluster_out;
        return Result::Success();
    } else {
        if (ci != NULL)
            ci->f_nextcluster = -1;
        return Result::Failure(ERANGE);
    }
}

/*
 * Sets a cluster value to a given value.
 */
static Result fat_set_cluster(struct VFS_MOUNTED_FS* fs, uint32_t cluster_num, uint32_t cluster_val)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    /* Calculate the block and offset within that block of the cluster */
    blocknr_t sector_num;
    uint32_t offset;
    fat_make_cluster_block_offset(fs, cluster_num, &sector_num, &offset);

    /* Fetch the FAT data */
    BIO* bio;
    if (auto result = vfs_bread(fs, sector_num, &bio); result.IsFailure())
        return result;

    switch (fs_privdata->fat_type) {
        case 16:
            FAT_TO_LE16((char*)(static_cast<char*>(bio->Data()) + offset), cluster_val);
            break;
        case 32: /* actually FAT-28... */
            FAT_TO_LE32((char*)(static_cast<char*>(bio->Data()) + offset), cluster_val);
            break;
        default:
            panic("unsuported fat type");
    }


    /* Sync all other FAT tables as well */
    for (int i = 1; i < fs_privdata->num_fats; i++) {
        sector_num += fs_privdata->num_fat_sectors;
        BIO* bio2;
        if (auto result = vfs_bread(fs, sector_num, &bio2); result.IsFailure()) {
            /* XXX we should free the cluster */
            bio->Release();
            kprintf("fat_set_cluster(): XXX leaking cluster %u\n", sector_num);
            return result;
        }
        memcpy(bio2->Data(), bio->Data(), fs_privdata->sector_size);
        bio2->Write();
    }
    bio->Write();

    return Result::Success();
}

/*
 * Obtains the first available cluster and marks it as being used.
 */
static Result fat_claim_avail_cluster(struct VFS_MOUNTED_FS* fs, uint32_t* cluster_out)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    /* XXX We do a dumb find-first on the filesystem for now */
    for (uint32_t clusterno = fs_privdata->next_avail_cluster;
         clusterno < fs_privdata->total_clusters; clusterno++) {
        /* Obtain the FAT block, if necessary */
        uint32_t offset;
        blocknr_t cur_block;
        fat_make_cluster_block_offset(fs, clusterno, &cur_block, &offset);

        BIO* bio;
        if (auto result = vfs_bread(fs, cur_block, &bio); result.IsFailure())
            return result;

        /* Obtain the cluster value */
        uint32_t val = -1;
        switch (fs_privdata->fat_type) {
            case 16:
                val = FAT_FROM_LE16((char*)(static_cast<char*>(bio->Data()) + offset));
                if (val == 0) { /* available cluster? */
                    /* Yes; claim it */
                    FAT_TO_LE16((char*)(static_cast<char*>(bio->Data()) + offset), 0xfff8);
                }
                break;
            case 32: /* actually FAT-28... */
                val =
                    FAT_FROM_LE32((char*)(static_cast<char*>(bio->Data()) + offset)) & 0xfffffff;
                if (val == 0) { /* available cluster? */
                    /* Yes; claim it */
                    FAT_TO_LE32((char*)(static_cast<char*>(bio->Data()) + offset), 0xffffff8);
                }
                break;
        }
        if (val != 0) {
            bio->Release();
            continue;
        }

        /* OK; this one is available (and we just claimed it) - flush the FAT entry */
        bio->Write();

        /* XXX should update second fat */
        *cluster_out = clusterno;
        return Result::Success();
    }

    /* Out of available clusters */
    return Result::Failure(ENOSPC);
}

/*
 * Appends a new cluster to an inode; returns the next cluster.
 */
static Result fat_append_cluster(INode& inode, uint32_t* cluster_out)
{
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    /*
     * Figure out the last cluster of the file; we cache this per inode as we don't
     * want to clutter the cluster-cache with it (these entries need to be updated
     * if we append a cluster)
     */
    uint32_t last_cluster = privdata->last_cluster;
    if (last_cluster == 0) {
        Result result = fat_get_cluster(fs, privdata->first_cluster, (uint32_t)-1, &last_cluster);
        if (result.IsFailure() && result.AsErrno() != ERANGE) {
            panic("unable to obtain last cluster");
        }
        privdata->last_cluster = last_cluster;
    }

    /* Obtain the next cluster - this will also mark it as being in use */
    uint32_t new_cluster = 0;
    if (auto result = fat_claim_avail_cluster(fs, &new_cluster); result.IsFailure())
        return result;

    /* If the file didn't have any clusters before, it sure does now */
    if (privdata->first_cluster == 0) {
        privdata->first_cluster = new_cluster;
        vfs_set_inode_dirty(inode);
    } else {
        /* Append this cluster to the file chain */
        if (auto result = fat_set_cluster(fs, last_cluster, new_cluster); result.IsFailure())
            return result; // XXX leaks clusterno
    }
    *cluster_out = new_cluster;

    /*
     * Update the cache; if there is an entry for our inode which is marked as
     * nonexistent, we expect it to be the last one and overwrite it.
     */
    {
        SpinlockGuard g(fs_privdata->spl_cache);
        for (int cache_item = 0; cache_item < FAT_NUM_CACHEITEMS; cache_item++) {
            struct FAT_CLUSTER_CACHEITEM* ci = &fs_privdata->cluster_cache[cache_item];
            if (ci->f_clusterno == privdata->first_cluster && ci->f_nextcluster == -1) {
                /* Found empty item - use it (we assume this always occurs at the end of the items)
                 */
                KASSERT(
                    ci->f_index ==
                        (inode.i_sb.st_size +
                         ((fs_privdata->sector_size * fs_privdata->sectors_per_cluster) - 1)) /
                            (fs_privdata->sector_size * fs_privdata->sectors_per_cluster),
                    "empty cache item isn't final item?");
                ci->f_nextcluster = new_cluster;
                break;
            }
        }
    }

    /* Update the block count of the inode */
    privdata->last_cluster = new_cluster;
    inode.i_sb.st_blocks += fs_privdata->sectors_per_cluster;
    return Result::Success();
}

Result fat_truncate_clusterchain(INode& inode)
{
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    /*
     * We need to free the clusters in reverse order: if we do it sequentially,
     * we won't be able to find the next one as we broke the chain...
     */
    unsigned int bytes_per_cluster = fs_privdata->sector_size * fs_privdata->sectors_per_cluster;
    int num_clusters = (inode.i_sb.st_size + bytes_per_cluster - 1) / bytes_per_cluster;

    Result result = Result::Success();
    uint32_t cluster = 0;
    for (int num = num_clusters - 1; num >= 0; num--) {
        {
            result = fat_get_cluster(fs, privdata->first_cluster, num, &cluster);
            if (result.IsFailure()) {
                if (result.AsErrno() == ERANGE)
                    break;     /* end of the run */
                return result; /* anything else is bad */
            }
        }

        /*
         * Throw away this cluster; note that fat_set_cluster() will not update the
         * cluster map, which is fine as we'll just flush the cache soon.
         */
        result = fat_set_cluster(fs, cluster, 0);
        if (result.IsFailure())
            break;
    }

    /*
     * Throw away the cluster map of this inode - we clean up everything even in
     * case of an error as it won't hurt to do so (and we expect little failure)
     */
    if (privdata->first_cluster > 0)
        fat_clear_cache(fs, privdata->first_cluster);
    return result;
}

/*
 * Maps the given block of an inode to the block device's block to use; note
 * that in our FAT implementation, a block size is identical to a sector size
 * (this is necessary because the data may not begin at a sector number
 * multiple of a cluster size)
 */
Result fat_block_map(INode& inode, blocknr_t block_in, blocknr_t& block_out, bool create)
{
    auto privdata = static_cast<struct FAT_INODE_PRIVDATA*>(inode.i_privdata);
    struct VFS_MOUNTED_FS* fs = inode.i_fs;
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);

    /*
     * FAT16 root inodes are special as they have a fixed location and
     * length; the FAT isn't needed to traverse them.
     */
    blocknr_t want_block;
    if (privdata->root_inode && fs_privdata->fat_type != 32) {
        want_block = fs_privdata->first_rootdir_sector + block_in;
        if (block_in >= (fs_privdata->num_rootdir_sectors * fs_privdata->sector_size))
            /* Cannot expand the root directory */
            return Result::Failure(ERANGE);
    } else {
        uint32_t cluster;
        Result result = fat_get_cluster(
            fs, privdata->first_cluster, block_in / fs_privdata->sectors_per_cluster, &cluster);
        if (result.IsFailure() && result.AsErrno() == ERANGE) {
            /* end of the chain */
            if (!create) {
                return result;
            }
            if (auto result = fat_append_cluster(inode, &cluster); result.IsFailure())
                return result;
        } else if (result.IsSuccess()) {
            KASSERT(
                create == 0,
                "request to create block that already exists (blocknum=%u, cluster=%u)",
                (int)(block_in / fs_privdata->sectors_per_cluster), cluster);
        } else {
            return result;
        }

        want_block = fat_cluster_to_sector(fs, cluster);
        want_block += block_in % fs_privdata->sectors_per_cluster;
    }

    block_out = want_block;
    return Result::Success();
}

int fat_clear_cache(struct VFS_MOUNTED_FS* fs, uint32_t first_cluster)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
    SpinlockGuard g(fs_privdata->spl_cache);

    /*
     * Throw away al inode's cluster items; this works by searching for the last
     * item in the cluster cache and copying it over our removed blocks - this
     * works because the cache isn't sorted.
     */
    unsigned int last_index = 0;
    while (last_index < FAT_NUM_CACHEITEMS &&
           fs_privdata->cluster_cache[last_index].f_clusterno != 0)
        last_index++;
    /* Cacheitems 0 ... i, for all i < last_index are in use */

    /* Throw away all inode's cluster items in the cache */
    int num_removed = 0;
    struct FAT_CLUSTER_CACHEITEM* ci = fs_privdata->cluster_cache;
    for (unsigned int n = 0; n < FAT_NUM_CACHEITEMS; n++, ci++) {
        if (ci->f_clusterno != first_cluster)
            continue;

        /*
         * OK, this is an item we'll have to destroy - copy the last entry over it and remove that
         * one. If this is the final entry, we skip the copying.
         */
        KASSERT(
            last_index >= 0 && last_index <= FAT_NUM_CACHEITEMS, "last index %u invalid",
            last_index);
        KASSERT(n <= last_index, "item to remove %u is beyond last index %i", n, last_index);
        if (n != (last_index - 1)) {
            /* Need to overwrite this item with the final cache item */
            memcpy(ci, &fs_privdata->cluster_cache[last_index - 1], sizeof(*ci));
            memset(&fs_privdata->cluster_cache[last_index - 1], 0, sizeof(*ci));
            last_index--;
        } else {
            memset(ci, 0, sizeof(*ci));
        }
        num_removed++;
    }
    return num_removed;
}

void fat_dump_cache(struct VFS_MOUNTED_FS* fs)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
    struct FAT_CLUSTER_CACHEITEM* ci = fs_privdata->cluster_cache;
    for (unsigned int n = 0; n < FAT_NUM_CACHEITEMS; n++, ci++) {
        kprintf(
            "ci[%i]: clusterno=%i, index=%i, nextcl=%u\n", n, ci->f_clusterno, ci->f_index,
            ci->f_nextcluster);
    }
}

Result fat_update_infosector(struct VFS_MOUNTED_FS* fs)
{
    auto fs_privdata = static_cast<struct FAT_FS_PRIVDATA*>(fs->fs_privdata);
    if (fs_privdata->infosector_num == 0)
        return Result::Success(); /* no info sector; nothing to do */

    /* If we have nothing sensible to store, don't bother */
    if (fs_privdata->next_avail_cluster < 2 || fs_privdata->num_avail_clusters == (uint32_t)-1)
        return Result::Success();

    BIO* bio;
    if (auto result = vfs_bread(fs, fs_privdata->infosector_num, &bio); result.IsFailure())
        return result;

    /*
     * We don't do any verification (this should have been done upon mount time)
     * and just put whatever values we currently have in there, if they are
     * valid.
     */
    auto fsi = reinterpret_cast<struct FAT_FAT32_FSINFO*>(bio->Data());
    FAT_TO_LE32(fsi->fsi_next_free, fs_privdata->next_avail_cluster);
    FAT_TO_LE32(fsi->fsi_free_count, fs_privdata->num_avail_clusters);
    bio->Write(); /* XXX even if we updated nothing? */

    return Result::Success();
}
