/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include <ananas/types.h>
#include <ananas/util/list.h>
#include "kernel/condvar.h"
#include "kernel/lock.h"
#include "kernel/result.h"

/* Size of a sector; any BIO block must be a multiple of this */
static constexpr inline unsigned int BIO_SECTOR_SIZE = 512;

class Device;
struct INode;

namespace bio
{
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
        using BIOBucketNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<BucketNode<T>>;
        template<typename T>
        using BIOChainNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<ChainNode<T>>;

    } // namespace internal

} // namespace bio

struct BIO;
typedef util::List<BIO, bio::internal::BIOBucketNodeAccessor<BIO>> BIOBucketList;
typedef util::List<BIO, bio::internal::BIOChainNodeAccessor<BIO>> BIOChainList;

/*
 * A basic I/O buffer, the root of all I/O requests. Locking is special here
 * as there are different users. We have:
 *
 * [b] Buffer owner, whoever holds CFLAG_BUSY
 * [c] Cache lock, mtx_cache
 * [o] Object mutex
 */
struct BIO {
    int b_cflags = 0;                       // [c] Flags, protected by mtx_cache
    int b_oflags = 0;                       // [o] Flags, protected by objlock
    Device* b_device = nullptr;             // [b] Device I/O'ing from
    blocknr_t b_block = 0;                  // [b] Block number to I/O
    blocknr_t b_ioblock = 0;                // [b] Translated block number to I/O
    unsigned int b_length = 0;              // [b] Length in bytes
    void* b_data = nullptr;                 // [b] Pointer to BIO data */
    Result b_status = Result::Success();    // [b] Last status
    Mutex* b_objlock = nullptr;             // [o] Associated lock
    ConditionVariable b_cv_done{"biodone"}; // [o] Condition variable for done
    ConditionVariable b_cv_busy{"biobusy"}; // [c] Condition variable for busy

    util::List<BIO>::Node b_NodeBucket /* Bucket list */;
    util::List<BIO>::Node b_NodeChain; /* Chain list */

    void* Data() { return b_data; }

    void Release();    // brelse()
    Result Write();    // brwrite()
    Result Wait();     // biowait()
    void Done(Result); // biodone()
};

Result bread(Device* device, blocknr_t block, size_t len, BIO*& result);
Result bwrite(BIO& bio);

void bsync();
