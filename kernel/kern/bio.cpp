/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2019 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */

/*
 * I/O buffer cache. This is greatly influenced by NetBSD's implementation,
 * which in turn is inspired by the original UNIX implementation as specified
 * in "The Design Of The Unix Operating System" by Maurice J. Bach.
 *
 * A few things to note:
 * - BIO's are always in a bucket queue, and may or may not be on the freelist
 *   (this is in line with [Bach])
 * - There are three locks (details are in the header file)
 * - b_objlock is intended to become the INode lock
 *   Currently, this is yet to be implemented
 * - We do not have a relation to INode's yet
 */
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/pcpu.h"
#include "kernel/init.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/pool.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vfs/types.h"
#include <ananas/util/array.h>

namespace
{
    // Number of buckets used to hash a block number to
    constexpr int NumberOfBuckets = 16;

    // Number of BIO buffers available
    constexpr int NumberOfBuffers = 1024;

    // Number of BIO pools, from BIO_SECTOR_SIZE upwards
    inline constexpr auto NumberOfPools = 2;

    constexpr unsigned int BucketForBlock(blocknr_t block) { return block % NumberOfBuckets; }

    constexpr unsigned int BucketForBIO(const BIO& bio) { return BucketForBlock(bio.b_block); }

    BIOChainList bio_freelist;
    util::array<pool::Pool*, NumberOfPools> bio_pool;
    util::array<BIOBucketList, NumberOfBuckets> bio_bucket;

    Mutex mtx_cache{"biocache"};
    Mutex mtx_buffer{"biobuf"};

    ConditionVariable cv_buffer_needed{"bioneeded"};

    namespace cflag
    {
        constexpr int Busy = (1 << 0);    // Also known as locked
        constexpr int Invalid = (1 << 1); // Data is invalid
    }                                     // namespace cflag

    namespace oflag
    {
        constexpr int Done(1 << 0); // I/O completed
    }

    constexpr auto Calculate2Log(size_t n)
    {
        size_t result{};
        --n;
        for(; n > 0; n /= 2, ++result)
            ;
        return result;
    }

    constexpr auto bioSectorSize2log = Calculate2Log(BIO_SECTOR_SIZE);

    auto& GetPoolForLength(size_t len)
    {
        size_t index = Calculate2Log(len);
        index -= bioSectorSize2log;

        KASSERT(index < bio_pool.size(), "invalid index %d", index);
        return *bio_pool[index];
    }

    bool AllocateData(BIO& bio, size_t len)
    {
        mtx_cache.AssertLocked();

        auto& pool = GetPoolForLength(len);
        bio.b_data = pool.AllocateItem();
        bio.b_length = len;
        return true;
    }

    void FreeData(BIO& bio)
    {
        mtx_cache.AssertLocked();
        if (bio.b_data == nullptr)
            return; // already freed

        auto& pool = GetPoolForLength(bio.b_length);
        pool.FreeItem(bio.b_data);
        bio.b_data = nullptr;
        bio.b_length = 0;
    }

    BIO* GetNewBuffer()
    {
        mtx_cache.AssertLocked();

        if (bio_freelist.empty())
            return nullptr;

        auto& bio = bio_freelist.front();
        bio_freelist.pop_front();

        bio.b_cflags = cflag::Busy;
        bio.b_oflags = 0;
        bio.b_status = Result::Success();
        bio.b_objlock = &mtx_buffer;
        return &bio;
    }

    void CleanupBIO(BIO& bio)
    {
        mtx_cache.AssertLocked();

        FreeData(bio);
        bio.b_cflags |= cflag::Busy | cflag::Invalid;
        mtx_cache.Unlock();
        bio.Release();
        mtx_cache.Lock();
    }

    void CleanupBuffers()
    {
        mtx_cache.AssertLocked();

        // Grab the first thing from the free list that contains data
        for (auto& bio : bio_freelist) {
            if (bio.b_data == nullptr) {
                continue;
            }

            bio_freelist.remove(bio);
            CleanupBIO(bio);
            return;
        }
        kprintf("CleanupBIO: nothing to clean\n");
    }

    BIO* incore(Device* device, blocknr_t block)
    {
        mtx_cache.AssertLocked();

        // See if we can find the block in the bucket queue; if so, we can just return it
        auto& bucket = bio_bucket[BucketForBlock(block)];
        for (auto& bio : bucket) {
            if (bio.b_device != device || bio.b_block != block)
                continue;

            if (bio.b_cflags & cflag::Invalid)
                continue; // never return blocks with invalid data

            return &bio;
        }

        return nullptr;
    }

    /*
     * Return a given bio buffer. This will use any cached item if possible, or
     * allocate a new one as required (Bach, p44). The resulting BIO is always
     * locked (BUSY)
     */
    BIO& getblk(Device* device, blocknr_t block, size_t len)
    {
        KASSERT((len % BIO_SECTOR_SIZE) == 0, "length %u not a multiple of bio sector size", len);

        mtx_cache.Lock();
    bio_restart:
        if (auto bio = incore(device, block); bio != nullptr) {
            // Block found in the cache
            KASSERT(
                bio->b_length == len, "bio item found with length %u, requested length %u",
                bio->b_length, len); /* XXX should avoid... somehow */
            KASSERT(bio->b_data != nullptr, "bio in cache without data");

            if (bio->b_cflags & cflag::Busy) {
                // Bio is busy - wait until the owner calls Release()
                bio->b_cv_busy.Wait(mtx_cache);
                goto bio_restart;
            }
            bio->b_cflags |= cflag::Busy;

            // The bio was not busy, so it must be on the freelist - claim it
            // from there so that active bio's are never freed
            bio_freelist.remove(*bio);
            mtx_cache.Unlock();
            return *bio;
        }

        // Block is not on the bucket queue
        BIO* newBio = GetNewBuffer();
        if (newBio == nullptr) {
            // No bio's available; wait until anything becomes free XXX timeout?
            cv_buffer_needed.Wait(mtx_cache);
            goto bio_restart;
        }
        BIO& bio = *newBio;

        // Remove buffer from old queue
        bio_bucket[BucketForBIO(bio)].remove(bio);

        // Arrange storage
        if (bio.b_data == nullptr || bio.b_length != len) {
            FreeData(bio);
            while (!AllocateData(bio, len)) {
                // Out of data buffers; make room and retry
                CleanupBuffers();
            }
        }

        // Set up the bio; flags are filled out by GetNewBuffer()
        bio.b_device = device;
        bio.b_block = block;
        bio.b_ioblock = block;
        // bio.b_length is filled out by AllocateData()

        // Put buffer on corresponding queue
        bio_bucket[BucketForBIO(bio)].push_front(bio);
        mtx_cache.Unlock();
        return bio;
    }

} // unnamed namespace

const init::OnInit initBIO(init::SubSystem::BIO, init::Order::First, []() {
    // Allocate pools
    {
        size_t poolSize = BIO_SECTOR_SIZE;
        for(size_t n = 0; n < NumberOfPools; ++n) {
            char name[32];
            sprintf(name, "bio%db", poolSize);
            bio_pool[n] = new pool::Pool(name, poolSize);
            poolSize *= 2;
        }
    }

    // Initialize bio buffers and hook them up to the freelist/queue
    {
        struct BIO* bios = new BIO[NumberOfBuffers];
        for (unsigned int i = 0; i < NumberOfBuffers; i++, bios++) {
            BIO& bio = *bios;
            bio_freelist.push_back(bio);
            bio_bucket[BucketForBIO(bio)].push_back(bio);
        }
    }
});

Result BIO::Wait()
{

    b_objlock->Lock();
    while ((b_oflags & oflag::Done) == 0) {
        b_cv_done.Wait(*b_objlock);
    }
    b_objlock->Unlock();

    return b_status;
}

/*
 * Called by BIO consumers when they are done with a bio buffer - brelse()
 */
void BIO::Release()
{
    KASSERT((b_cflags & cflag::Busy) != 0, "release on non-busy bio %p", this);

    // Wake up event: waiting for any buffer to become free
    cv_buffer_needed.Signal();

    // If the I/O operation failed, mark the data as invalid
    {
        MutexGuard g(mtx_cache);
        if (b_status.IsFailure())
            b_cflags |= cflag::Invalid;

        if (b_cflags & cflag::Invalid) {
            bio_freelist.push_front(*this);
        } else {
            bio_freelist.push_back(*this);
        }
        b_cflags &= ~cflag::Busy;
    }

    // Wake up event: waiting for this buffer to become free
    b_cv_busy.Broadcast();
}

Result bread(Device* device, blocknr_t block, size_t len, BIO*& result)
{
    BIO& bio = getblk(device, block, len);

    if ((bio.b_oflags & oflag::Done) == 0) {
        // Kick the device; we want it to read
        device->GetBIODeviceOperations()->ReadBIO(bio);
    }

    // Sleep on event: disk read completed
    bio.Wait();
    result = &bio;
    return bio.b_status;
}

void BIO::Done(Result status)
{
    b_objlock->Lock();
    KASSERT((b_oflags & oflag::Done) == 0, "bio %p already done", this);
    b_oflags |= oflag::Done;
    b_status = status;

    bool async = false;
    if (async) {
        b_objlock->Unlock();
        Release();
    } else {
        b_objlock->Unlock();
        b_cv_done.Broadcast();
    }
}

Result BIO::Write()
{
    KASSERT((b_cflags & cflag::Busy) != 0, "buffer not busy");

    // Update request: set as write and not done yet
    b_status = Result::Success();
    {
        MutexGuard g(*b_objlock);
        b_oflags &= ~oflag::Done;
    }

    // Initiate disk write
    b_device->GetBIODeviceOperations()->WriteBIO(*this);

    // Wait for the result XXX sync only
    auto result = Wait();
    Release();
    return result;
}

void bsync()
{
    // TODO - we do everything sync now so this does not matter
}
