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
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vfs/types.h"
#include "options.h"

TRACE_SETUP;

namespace
{
    // Amount of data available for BIO data, in bytes
    constexpr int DataSize = 512 * 1024;

    // Number of buckets used to hash a block number to
    constexpr int NumberOfBuckets = 16;

    // Number of BIO buffers available
    constexpr int NumberOfBuffers = 1024;

    constexpr unsigned int BucketForBlock(blocknr_t block) { return block % NumberOfBuckets; }

    constexpr unsigned int BucketForBIO(const BIO& bio) { return BucketForBlock(bio.b_block); }

    BIOChainList bio_freelist;
    util::array<BIOBucketList, NumberOfBuckets> bio_bucket;
    unsigned int bio_bitmap_size;
    uint8_t* bio_bitmap = NULL;
    uint8_t* bio_data = NULL;

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

    bool AllocateData(BIO& bio, size_t len)
    {
        mtx_cache.AssertLocked();

        // Find available data blocks in the bio data pool
        unsigned int chain_length = 0, cur_data_block = 0;
        unsigned int num_blocks = len / BIO_SECTOR_SIZE;
        for (; cur_data_block < bio_bitmap_size * 8; cur_data_block++) {
            if (bio_bitmap[cur_data_block / 8] & (1 << (cur_data_block % 8))) {
                // Data block isn't available - try again
                chain_length = 0;
                continue;
            }

            // Item is available; stop if we have enough blocks
            if (chain_length == num_blocks)
                break;
            chain_length++;
        }
        if (chain_length != num_blocks) {
            // No bio data available; clean up some and retry
            return false;
        }

        // Walk back the number of blocks we need to allocate, we know they are free
        cur_data_block -= chain_length;

        // Mark the blocks as in use
        for (unsigned int n = cur_data_block; n < cur_data_block + num_blocks; n++) {
            bio_bitmap[n / 8] |= (1 << (n % 8));
        }

        // Hook them to the bio
        bio.b_data = bio_data + (cur_data_block * BIO_SECTOR_SIZE);
        return true;
    }

    void FreeData(BIO& bio)
    {
        mtx_cache.AssertLocked();
        if (bio.b_data == nullptr)
            return; // already freed

        unsigned int bio_data_block =
            (static_cast<uint8_t*>(bio.Data()) - static_cast<uint8_t*>(bio_data)) / BIO_SECTOR_SIZE;
        for (unsigned int n = bio_data_block; n < bio_data_block + (bio.b_length / BIO_SECTOR_SIZE);
             n++) {
            KASSERT((bio_bitmap[n / 8] & (1 << (n % 8))) != 0, "data block %u not assigned", n);
            bio_bitmap[n / 8] &= ~(1 << (n % 8));
        }
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
        TRACE(BIO, FUNC, "dev=%p, block=%u, len=%u", device, (int)block, len);
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
        bio.b_length = len;

        // Put buffer on corresponding queue
        bio_bucket[BucketForBIO(bio)].push_front(bio);
        mtx_cache.Unlock();
        TRACE(BIO, INFO, "returning new bio=%p", &bio);
        return bio;
    }

} // unnamed namespace

const init::OnInit initBIO(init::SubSystem::BIO, init::Order::First, []() {
    /*
     * Allocate the BIO data buffers. Note that we allocate a BIO_SECTOR_SIZE - 1
     * more than needed if necessary - this is because we want to keep the data
     * bitmap at the beginning to prevent it from being messed with by faulty
     * drivers; and this will ensure it to be adequately aligned as a nice bonus.
     */
    bio_bitmap_size = ((DataSize / BIO_SECTOR_SIZE) + 7) / 8; /* in bytes */
    int bio_bitmap_slack = 0;
    if ((bio_bitmap_size & (BIO_SECTOR_SIZE - 1)) > 0)
        bio_bitmap_slack = BIO_SECTOR_SIZE - (bio_bitmap_size % BIO_SECTOR_SIZE);
    bio_bitmap = new uint8_t[DataSize + bio_bitmap_size + bio_bitmap_slack];
    bio_data = bio_bitmap + bio_bitmap_size + bio_bitmap_slack;

    // Initialize bio buffers and hook them up to the freelist/queue
    {
        struct BIO* bios = new BIO[NumberOfBuffers];
        for (unsigned int i = 0; i < NumberOfBuffers; i++, bios++) {
            BIO& bio = *bios;
            bio_freelist.push_back(bio);
            bio_bucket[BucketForBIO(bio)].push_back(bio);
        }
    }

    /*
     * Construct the BIO bitmap; we need to mark all items as clear except those
     * in the slack section, they cannot be used.
     */
    memset(bio_bitmap, 0, bio_bitmap_size);
    if (((DataSize / BIO_SECTOR_SIZE) & 7) != 0) {
        /*
         * This means the bitmap size is not a multiple of a byte; we need to set
         * the trailing bits to prevent buffers from being allocated that do not
         * exist.
         */
        for (int n = (DataSize / BIO_SECTOR_SIZE) & 7; n < 8; n++)
            bio_bitmap[bio_bitmap_size] |= (1 << n);
    }
});

Result BIO::Wait()
{
    TRACE(BIO, FUNC, "bio=%p", this);

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
    TRACE(BIO, FUNC, "bio=%p", this);
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
    TRACE(BIO, FUNC, "bio=%p DONE", this);
}

Result bread(Device* device, blocknr_t block, size_t len, BIO*& result)
{
    BIO& bio = getblk(device, block, len);

    if ((bio.b_oflags & oflag::Done) == 0) {
        // Kick the device; we want it to read
        if (auto result = device->GetBIODeviceOperations()->ReadBIO(bio); result.IsFailure()) {
            kprintf("bio_read(): device_read() failed, %i\n", result.AsStatusCode());
            return result;
        }
    }

    // Sleep on event: disk read completed
    bio.Wait();
    TRACE(BIO, INFO, "dev=%p, block=%u, len=%u ==> new block %p", device, (int)block, len, &bio);
    result = &bio;
    return bio.b_status;
}

Result bwrite(BIO& bio)
{
    panic("bwrite");
    // TODO: Initiate disk write
    if (/* TODO: synchronous */ true) {
    } else if (/* TODO: marked for delayed write */ false) {
        // TODO: mark buffer to put at head of free list
    }
}

void BIO::Done()
{
    TRACE(BIO, FUNC, "bio=%p", this);

    b_objlock->Lock();
    KASSERT((b_oflags & oflag::Done) == 0, "bio %p already done", this);
    b_oflags |= oflag::Done;

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

    // Initiate disk write XXX This should not fail
    {
        auto result = b_device->GetBIODeviceOperations()->WriteBIO(*this);
        KASSERT(result.IsSuccess(), "writebio failed?! %d", result.AsStatusCode());
    }

    // Wait for the result XXX sync only
    auto result = Wait();
    Release();
    return result;
}

void bsync()
{
    // TODO - we do everything sync now so this does not matter
}

#ifdef OPTION_KDB
const kdb::RegisterCommand kdbBio("bio", "Display I/O buffers", [](int, const kdb::Argument*) {
    kprintf("bio dump\n");

    unsigned int freelist_avail = 0;
    for (auto& bio : bio_freelist) {
        freelist_avail++;
    }
    kprintf("lists: %u total, %u bio's free\n", NumberOfBuffers, freelist_avail);

    kprintf("buckets:");
    for (unsigned int bucket_num = 0; bucket_num < NumberOfBuckets; bucket_num++) {
        kprintf("%u =>", bucket_num);
        {
            if (!bio_bucket[bucket_num].empty()) {
                for (auto& bio : bio_bucket[bucket_num])
                    kprintf(" %u(%d)", bio.b_block, bio.b_cflags);
            } else {
                kprintf(" (empty)");
            }
            kprintf("\n");
        }
    }

    unsigned int databuf_avail = 0;
    kprintf("data buffers in use:");
    {
        int current = -1;
        for (unsigned int i = 0; i < bio_bitmap_size * 8; i++) {
            if ((bio_bitmap[i / 8] & (1 << (i % 8))) != 0) {
                /* This entry is used; need to show it */
                if (current == -1)
                    kprintf(" %u", i);
                current = i;
                continue;
            }
            if (current != -1) {
                kprintf("-%u", i);
                current = -1;
            }
            databuf_avail++;
        }
        if (current != -1)
            kprintf("-%u\n", bio_bitmap_size * 8);
    }
    kprintf(", available: %u out of %u total\n", databuf_avail, DataSize / BIO_SECTOR_SIZE);
});
#endif /* KDB */
