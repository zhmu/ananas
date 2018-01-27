#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/trace.h"
#include "options.h"

TRACE_SETUP;

struct BIOBucket {
	Spinlock b_Lock;
	BIOBucketList b_List;
};

static BIOChainList bio_freelist;
static BIOChainList bio_usedlist;
static BIOBucket bio_bucket[BIO_BUCKET_SIZE];
static unsigned int bio_bitmap_size;
static uint8_t* bio_bitmap = NULL;
static uint8_t* bio_data = NULL;

static Spinlock spl_bio_lists;
static Spinlock spl_bio_bitmap;

static errorcode_t
bio_init()
{
	/*
	 * Allocate the BIO data buffers. Note that we allocate a BIO_SECTOR_SIZE - 1
	 * more than needed if necessary - this is because we want to keep the data
	 * bitmap at the beginning to prevent it from being messed with by faulty
	 * drivers; and this will ensure it to be adequately aligned as a nice bonus.
	 */
	bio_bitmap_size = ((BIO_DATA_SIZE / BIO_SECTOR_SIZE) + 7) / 8; /* in bytes */
	int bio_bitmap_slack = 0;
	if ((bio_bitmap_size & (BIO_SECTOR_SIZE - 1)) > 0)
		bio_bitmap_slack = BIO_SECTOR_SIZE - (bio_bitmap_size % BIO_SECTOR_SIZE);
	bio_bitmap = new uint8_t[BIO_DATA_SIZE + bio_bitmap_size + bio_bitmap_slack];
	bio_data = bio_bitmap + bio_bitmap_size + bio_bitmap_slack;

	/* Clear the BIO bucket chain */
	for (unsigned int i = 0; i < BIO_BUCKET_SIZE; i++) {
		spinlock_init(bio_bucket[i].b_Lock);
		bio_bucket[i].b_List.clear();
	}

	/* Initialize bio buffers and hook them up to the freelist */
	struct BIO* bios = new BIO[BIO_NUM_BUFFERS];
	for (unsigned int i = 0; i < BIO_NUM_BUFFERS; i++, bios++) {
		sem_init(bios->sem, 1);
		bio_freelist.push_back(*bios);
	}

	/*
	 * Construct the BIO bitmap; we need to mark all items as clear except those
	 * in the slack section, they cannot be used.
	 */
	memset(bio_bitmap, 0, bio_bitmap_size);
	if (((BIO_DATA_SIZE / BIO_SECTOR_SIZE) & 7) != 0) {
		/*
	 	 * This means the bitmap size is not a multiple of a byte; we need to set
		 * the trailing bits to prevent buffers from being allocated that do not
		 * exist.
	 	 */
		for (int n = (BIO_DATA_SIZE / BIO_SECTOR_SIZE) & 7; n < 8; n++)
			bio_bitmap[bio_bitmap_size] |= (1 << n);
	}
	spinlock_init(spl_bio_lists);
	spinlock_init(spl_bio_bitmap);
	return ananas_success();
}

INIT_FUNCTION(bio_init, SUBSYSTEM_BIO, ORDER_FIRST);

static void
bio_waitcomplete(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);
	while((bio.flags & BIO_FLAG_PENDING) != 0) {
		sem_wait(bio.sem);
	}
}

static void
bio_waitdirty(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);
	while((bio.flags & BIO_FLAG_DIRTY) != 0) {
		sem_wait(bio.sem);
	}
}

/*
 * Called to queue a bio to storage; called with list lock held.
 */
static void
bio_flush(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);

	/* If the bio isn't dirty, we needn't write it so just return */
	if ((bio.flags & BIO_FLAG_DIRTY) == 0)
		return;

	TRACE(BIO, INFO, "bio %p (lba %u) is dirty, flushing", &bio, (uint32_t)bio.io_block);

	errorcode_t err = bio.device->GetBIODeviceOperations()->WriteBIO(bio);
	if (ananas_is_failure(err)) {
		kprintf("bio_flush(): device_write() failed, %i\n", err);
		bio_set_error(bio);
	} else {
		/* Wait until we have something to report... */
		bio_waitdirty(bio);
		/*
		 * We're all set - the block I/O driver is responsible for clearing the dirty flag if
		 * necessary
		 */
	}
}

static void
bio_cleanup()
{
	TRACE(BIO, FUNC, "called");

	spinlock_lock(spl_bio_lists);
	KASSERT(!bio_usedlist.empty(), "usedlist is empty");

	/* Grab the final entry and remove it from the list */
	BIO& bio = bio_usedlist.back();
	bio_usedlist.pop_back();

	bio_flush(bio);

	/* Add it to the freelist */
	bio_freelist.push_back(bio);

	KASSERT(bio.data != NULL, "to-remove bio %p has no data (fl %x, block %x, len %x)",
	 &bio, bio.flags, (int)bio.block, bio.length);
	/*
	 * OK, now we need to nuke the data bio points to; this must be done within
	 * the lists spinlock because it must not be allocated by someone else.
	 */
	spinlock_lock(spl_bio_bitmap);
	unsigned int bio_data_block = (static_cast<uint8_t*>(bio.data) - static_cast<uint8_t*>(bio_data)) / BIO_SECTOR_SIZE;
	for (unsigned int n = bio_data_block; n < bio_data_block + (bio.length / BIO_SECTOR_SIZE); n++) {
		KASSERT((bio_bitmap[n / 8] & (1 << (n % 8))) != 0, "data block %u not assigned", n);
		bio_bitmap[n / 8] &= ~(1 << (n % 8));
	}
	/* Finally, remove the block from the bucket chain */
	unsigned int bucket_num = bio.block % BIO_BUCKET_SIZE;
	spinlock_lock(bio_bucket[bucket_num].b_Lock);
	KASSERT(!bio_bucket[bucket_num].b_List.empty(), "bio bucket %u is empty", bucket_num);
	bio_bucket[bucket_num].b_List.remove(bio);

	/*
	 * Clear the bio info; it's available again for use (but set it as pending as
	 * the data it's not yet available)
	 */
	bio.flags = BIO_FLAG_PENDING;
	bio.data = nullptr;
	spinlock_unlock(bio_bucket[bucket_num].b_Lock);
	spinlock_unlock(spl_bio_bitmap);
	spinlock_unlock(spl_bio_lists);
}

/*
 * Return a given bio buffer. This will use any cached item if possible, or
 * allocate a new one as required.
 */
static BIO*
bio_get_buffer(Ananas::Device* device, blocknr_t block, size_t len)
{
	TRACE(BIO, FUNC, "dev=%p, block=%u, len=%u", device, (int)block, len);
	KASSERT((len % BIO_SECTOR_SIZE) == 0, "length %u not a multiple of bio sector size", len);

	/* First of all, lock the corresponding queue */
	unsigned int bucket_num = block % BIO_BUCKET_SIZE;
bio_restart:
	spinlock_lock(bio_bucket[bucket_num].b_Lock);

	/* See if we can find the block in the bucket queue; if so, we can just return it */
	for(auto& bio: bio_bucket[bucket_num].b_List) {
		if (bio.device != device || bio.block != block)
			continue;

		/*
	 	 * This is the block we need; we must move it in front of the used queue to
		 * prevent it from being nuked.
		 */
		spinlock_lock(spl_bio_lists);
		KASSERT(!bio_usedlist.empty(), "usedlist is empty");
		/* Remove ourselves from the chain... */
		bio_usedlist.remove(bio);
		/* ...and prepend us at the beginning */
		bio_usedlist.push_front(bio);
		spinlock_unlock(spl_bio_lists);

		spinlock_unlock(bio_bucket[bucket_num].b_Lock);
		KASSERT(bio.length == len, "bio item found with length %u, requested length %u", bio.length, len); /* XXX should avoid... somehow */

		/*
		 * We have already found the I/O buffer in the cache; however, if two
		 * threads request the same block at roughly the same time, one will
		 * be stuck waiting for it to be read and the other will end up here.
		 *
		 * To prevent this, we'll have to wait until the BIO buffer is no
		 * longer pending, as this ensures it will have been read.
		 *
		 * XXX What about the NODATA flag?
		 */
		bio_waitcomplete(bio);
		TRACE(BIO, INFO, "returning cached bio=%p", &bio);
		return &bio;
	}

	/* Grab a bio from the head of the freelist */
	spinlock_lock(spl_bio_lists);
	BIO* bio = nullptr;
	if (!bio_freelist.empty()) {
		bio = &bio_freelist.front();
		bio_freelist.pop_front();
	}

	if (bio == nullptr) {
		/* No bio's available; clean up some */
		spinlock_unlock(spl_bio_lists);
		spinlock_unlock(bio_bucket[bucket_num].b_Lock);
		bio_cleanup();
		goto bio_restart;
	}

	/* And add it to the used list */
	bio_usedlist.push_front(*bio);
	spinlock_unlock(spl_bio_lists);

	/* Find available data blocks in the bio data pool */
bio_restartdata:
	spinlock_lock(spl_bio_bitmap);
	unsigned int chain_length = 0, cur_data_block = 0;
	unsigned int num_blocks = len / BIO_SECTOR_SIZE;
	for (; cur_data_block < bio_bitmap_size * 8; cur_data_block++) {
		if(bio_bitmap[cur_data_block / 8] & (1 << (cur_data_block % 8))) {
			/* Data block isn't available - try again */
			chain_length = 0;
			continue;
		}

		if (chain_length == num_blocks)
			break;
		chain_length++;
	}
	if (chain_length != num_blocks) {
		/* No bio data available; clean up some */
		spinlock_unlock(spl_bio_bitmap);
		spinlock_unlock(bio_bucket[bucket_num].b_Lock);
		bio_cleanup();
		spinlock_lock(bio_bucket[bucket_num].b_Lock);
		goto bio_restartdata;
	}
	/* Walk back the number of blocks we need to allocate, we know they are free */
	cur_data_block -= chain_length;

	/* Mark the blocks as in use */
	for (unsigned int n = cur_data_block; n <  cur_data_block + num_blocks; n++) {
		bio_bitmap[n / 8] |= (1 << (n % 8));
	}
	spinlock_unlock(spl_bio_bitmap);

	/* Hook the request to the corresponding bucket */
	bio_bucket[bucket_num].b_List.push_front(*bio);
	spinlock_unlock(bio_bucket[bucket_num].b_Lock);

	/*
	 * Throw away any flags the buffer has (as this is a new request, we can't
	 * anything more sensible yet) - note that we need to set the pending flag
	 * because the data isn't ready yet.
	 */
	bio->flags = BIO_FLAG_PENDING;
	bio->device = device;
	bio->block = block;
	bio->io_block = block;
	bio->length = len;
	bio->data = bio_data + (cur_data_block * BIO_SECTOR_SIZE);
	TRACE(BIO, INFO, "returning new bio=%p", bio);
	return bio;
}

/*
 * Called by BIO consumers when they are done with a bio buffer.
 */
void
bio_free(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);
}

struct BIO*
bio_get(Ananas::Device* device, blocknr_t block, size_t len, int flags)
{
	struct BIO* bio = bio_get_buffer(device, block, len);
	if (flags & BIO_READ_NODATA) {
		/*
		 * The requester doesn't want the actual data; this means we needn't
	 	 * schedule the read or even bother to check whether it's pending
	 	 * or not; we just return the bio constructed above without it
		 * being pending - caller is likely to destroy any data in it
		 * either way.
		 */
		bio->flags &= ~BIO_FLAG_PENDING;
		return bio;
	}

	/*
	 * If we have a block that is not pending, we can just return it. Note that
	 * dirty blocks are never pending.
	 */
	if ((bio->flags & BIO_FLAG_PENDING) == 0) {
		TRACE(BIO, INFO, "dev=%p, block=%u, len=%u ==> cached block %p", device, (int)block, len, bio);
		return bio;
	}

	/* kick the device; we want it to read */
	errorcode_t err = device->GetBIODeviceOperations()->ReadBIO(*bio);
	if (ananas_is_failure(err)) {
		kprintf("bio_read(): device_read() failed, %i\n", err);
		bio_set_error(*bio);
		return bio;
	}

	/* ... and wait until we have something to report... */
	bio_waitcomplete(*bio);
	TRACE(BIO, INFO, "dev=%p, block=%u, len=%u ==> new block %p", device, (int)block, len, bio);
	return bio;
}

void
bio_set_error(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);
	bio.flags = (bio.flags & ~BIO_FLAG_PENDING) | BIO_FLAG_ERROR;
	sem_signal(bio.sem);
}

void
bio_set_available(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);
	bio.flags &= ~BIO_FLAG_PENDING;
	sem_signal(bio.sem);
}

void
bio_set_dirty(BIO& bio)
{
	TRACE(BIO, FUNC, "bio=%p", &bio);
	bio.flags |= BIO_FLAG_DIRTY;
	bio_flush(bio); /* XXX debug aid so that the image can be inspected */
}

#ifdef OPTION_KDB
KDB_COMMAND(bio, NULL, "Display I/O buffers")
{
	kprintf("bio dump\n");

	unsigned int freelist_avail = 0, usedlist_used = 0;
	spinlock_lock(spl_bio_lists);
	for(auto& bio: bio_freelist)
		freelist_avail++;
	for(auto& bio: bio_usedlist)
		usedlist_used++;
	spinlock_unlock(spl_bio_lists);
	kprintf("lists: %u bio's used, %u bio's free, %u total\n", usedlist_used, freelist_avail, BIO_NUM_BUFFERS);
	KASSERT(freelist_avail + usedlist_used == BIO_NUM_BUFFERS, "chain length does not add up");

	kprintf("buckets:");
	for (unsigned int bucket_num = 0; bucket_num < BIO_BUCKET_SIZE; bucket_num++) {
		kprintf("%u =>", bucket_num);
		spinlock_lock(bio_bucket[bucket_num].b_Lock);
		if(!bio_bucket[bucket_num].b_List.empty()) {
			for(auto& bio: bio_bucket[bucket_num].b_List)
				kprintf(" %p (%u)", &bio, bio.block);
		} else {
			kprintf(" (empty)");
		}
		kprintf("\n");
		spinlock_unlock(bio_bucket[bucket_num].b_Lock);
	}

	unsigned int databuf_avail = 0;
	kprintf("data buffers:");
	spinlock_lock(spl_bio_bitmap);
	int current = -1;
	for (unsigned int i = 0; i < bio_bitmap_size * 8; i++) {
		if((bio_bitmap[i / 8] & (1 << (i % 8))) != 0) {
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
	spinlock_unlock(spl_bio_bitmap);
	kprintf(" %u available (%u total)\n", databuf_avail, BIO_DATA_SIZE / BIO_SECTOR_SIZE);

	KASSERT(databuf_avail <= (BIO_DATA_SIZE / BIO_SECTOR_SIZE), "more bio data available than total");
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
