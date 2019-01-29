/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_BIO_H__
#define __ANANAS_BIO_H__

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/lock.h"

/* Number of buckets used to hash a block number to */
#define BIO_BUCKET_SIZE 16

/* Number of BIO buffers available */
#define BIO_NUM_BUFFERS 1024

/* Amount of data available for BIO data, in bytes */
#define BIO_DATA_SIZE (512 * 1024)

/* Size of a sector; any BIO block must be a multiple of this */
#define BIO_SECTOR_SIZE 512

#define BIO_IS_DIRTY(bio) ((bio)->flags & BIO_FLAG_DIRTY)
#define BIO_IS_READ(bio) ((bio)->flags & BIO_FLAG_READ)
#define BIO_IS_WRITE(bio) ((bio)->flags & BIO_FLAG_WRITE)
#define BIO_IS_ERROR(bio) ((bio)->flags & BIO_FLAG_ERROR)
#define BIO_DATA(bio) ((bio)->data)

class Device;

// Internal stuff so we can work with children and all nodes
namespace internal
{
    template<typename T>
    struct BucketNode {
        static typename util::List<T>::Node& Get(T& t) { return t.b_NodeBucket; }
    };

    template<typename T>
    struct ChainNode {
        static typename util::List<T>::Node& Get(T& t) { return t.b_NodeChain; }
    };

    template<typename T>
    using BIOBucketNodeAccessor = typename util::List<T>::template nodeptr_accessor<BucketNode<T>>;
    template<typename T>
    using BIOChainNodeAccessor = typename util::List<T>::template nodeptr_accessor<ChainNode<T>>;

} // namespace internal

struct BIO;
typedef util::List<BIO, internal::BIOBucketNodeAccessor<BIO>> BIOBucketList;
typedef util::List<BIO, internal::BIOChainNodeAccessor<BIO>> BIOChainList;

/*
 * A basic I/O buffer, the root of all I/O requests.
 */
struct BIO {
    uint32_t flags;
#define BIO_FLAG_PENDING 0x0001 /* Block is pending read */
#define BIO_FLAG_DIRTY 0x0002   /* I/O needs to be written */
#define BIO_FLAG_ERROR 0x8000   /* Request failed */
    Device* device;             /* Device I/O'ing from */
    blocknr_t block;            /* Block number to I/O */
    blocknr_t io_block;         /* Translated block number to I/O */
    unsigned int length;        /* Length in bytes (<= PAGE_SIZE, so int will do) */
    void* data;                 /* Pointer to BIO data */
    Semaphore sem{1};           /* Semaphore for this BIO */

    util::List<BIO>::Node b_NodeBucket /* Bucket list */;
    util::List<BIO>::Node b_NodeChain; /* Chain list */
};

/* Flags of BIO_READ */
#define BIO_READ_NODATA 0x0001 /* Caller is not interested in the data */

void bio_set_error(BIO& bio);
void bio_set_available(BIO& bio);
void bio_set_dirty(BIO& bio);
BIO* bio_get(Device* device, blocknr_t block, size_t len, int flags);

static inline BIO* bio_read(Device* device, blocknr_t block, size_t len)
{
    return bio_get(device, block, len, 0);
}

BIO& bio_get_next(Device* device);
void bio_free(BIO& bio);
void bio_dump();
void bio_sync();

#endif /* __ANANAS_BIO_H__ */
