#include <ananas/mm.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/lock.h>
#include <ananas/schedule.h>

static struct BIO* bio_freelist = NULL;
static struct BIO* bio_usedlist = NULL;
static struct BIO* bio_bucket[BIO_BUCKET_SIZE];
static unsigned int bio_bitmap_size;
static uint8_t* bio_bitmap = NULL;
static uint8_t* bio_data = NULL;

static struct SPINLOCK spl_bucket[BIO_BUCKET_SIZE];
static struct SPINLOCK spl_bio_lists;
static struct SPINLOCK spl_bio_bitmap;

void bio_dump();

void
bio_init()
{
	/*
	 * Allocate the BIO buffers and the BIO data buffers. Note that we allocate
	 * a BIO_SECTOR_SIZE - 1 more  than needed if necessary - this is because we
	 * want to keep the data bitmap at the beginning to prevent it from being
	 * messed with by faulty drivers; and this will ensure it to be adequately
	 * aligned as a nice bonus.
	 */
	bio_freelist = kmalloc(sizeof(struct BIO) * BIO_NUM_BUFFERS);
	bio_bitmap_size = ((BIO_DATA_SIZE / BIO_SECTOR_SIZE) + 7) / 8; /* in bytes */
	int bio_bitmap_slack = 0;
	if ((bio_bitmap_size & (BIO_SECTOR_SIZE - 1)) > 0)
		bio_bitmap_slack = BIO_SECTOR_SIZE - (bio_bitmap_size % BIO_SECTOR_SIZE);
	bio_bitmap = kmalloc(BIO_DATA_SIZE + bio_bitmap_size + bio_bitmap_slack);
	bio_data = bio_bitmap + bio_bitmap_size + bio_bitmap_slack;

	/* Clear the BIO bucket chain */
	for (unsigned int i = 0; i < BIO_BUCKET_SIZE; i++) {
		spinlock_init(&spl_bucket[i]);
		bio_bucket[i] = NULL;
	}

	/* Construct the BIO freelist pointer chain */
	memset(bio_freelist, 0, sizeof(struct BIO) * BIO_NUM_BUFFERS);
	for (unsigned int i = 0; i < BIO_NUM_BUFFERS; i++) {
		if (i > 0)
			bio_freelist[i].chain_prev = &bio_freelist[i - 1];
		else
			bio_freelist[i].chain_prev = &bio_freelist[BIO_NUM_BUFFERS - 1];
		if (i < BIO_NUM_BUFFERS - 1)
			bio_freelist[i].chain_next = &bio_freelist[i + 1];
		else
			bio_freelist[i].chain_next = NULL;
	}

	/*
	 * Construct the BIO bitmap; we need to mark all items as clear except those
	 * in the slack section, they cannot be used.
	 */
	memset(bio_bitmap, 0, bio_bitmap_size);
	if ((BIO_DATA_SIZE / BIO_SECTOR_SIZE) & 7 != 0) {
		/*
	 	 * This means the bitmap size is not a multiple of a byte; we need to set
		 * the trailing bits to prevent buffers from being allocated that do not
		 * exist.
	 	 */
		for (int n = (BIO_DATA_SIZE / BIO_SECTOR_SIZE) & 7; n < 8; n++)
			bio_bitmap[bio_bitmap_size] |= (1 << n);
	}
	spinlock_init(&spl_bio_lists);
	spinlock_init(&spl_bio_bitmap);
}

/*
 * Called to queue a bio to storage; called with list lock held.
 */
static void
bio_flush(struct BIO* bio)
{
}

static void
bio_cleanup()
{
	spinlock_lock(&spl_bio_lists);
	KASSERT(bio_usedlist != NULL, "usedlist is NULL");

	/* Grab the final entry and remove it from the list */
	struct BIO* bio = bio_usedlist->chain_prev;
	bio_flush(bio);
	bio_usedlist->chain_prev = bio_usedlist->chain_prev->chain_prev;
	bio_usedlist->chain_prev->chain_next = NULL;
	/* Add it to the freelist */
	if (bio_freelist != NULL) {
		bio->chain_prev = bio_freelist->chain_prev;
		bio_freelist->chain_prev = bio;
	} else {
		bio->chain_prev = NULL;
	}
	bio->chain_next = bio_freelist;
	bio_freelist = bio;
	KASSERT(bio->data != NULL, "to-remove bio %p has no data", bio);
	/*
	 * OK, now we need to nuke the data bio points to; this must be done within
	 * the lists spinlock because it must not be allocated by someone else.
	 */
	spinlock_lock(&spl_bio_bitmap);
	unsigned int bio_data_block = (bio->data - (void*)bio_data) / BIO_SECTOR_SIZE;
	for (unsigned int n = bio_data_block; n < bio_data_block + (bio->length / BIO_SECTOR_SIZE); n++) {
		KASSERT((bio_bitmap[n / 8] & (1 << (n % 8))) != 0, "data block %u not assigned", n);
		bio_bitmap[n / 8] &= ~(1 << (n % 8));
	}
	/* Finally, remove the block from the bucket chain */
	unsigned int bucket_num = bio->block % BIO_BUCKET_SIZE;
	spinlock_lock(&spl_bucket[bucket_num]);
	KASSERT(bio_bucket[bucket_num] != NULL, "bio bucket %u is NULL", bucket_num);
	if (bio_bucket[bucket_num] == bio) {
		bio_bucket[bucket_num] = bio->bucket_next;
		bio->bucket_prev = NULL;
	} else {
		if(bio->bucket_prev != NULL)
			bio->bucket_prev->bucket_next = bio->bucket_next;
		if(bio->bucket_next != NULL)
			bio->bucket_next->bucket_prev = bio->bucket_prev;
	}
	/* nuke the bio info */
	bio->flags = 0;
	bio->data = NULL;
	spinlock_unlock(&spl_bucket[bucket_num]);
	spinlock_unlock(&spl_bio_bitmap);
	spinlock_unlock(&spl_bio_lists);
}

/*
 * Return a given bio buffer. This will use any cached item if possible, or
 * allocate a new one as required.
 */
static struct BIO*
bio_get(device_t dev, block_t block, size_t len)
{
	KASSERT((len % BIO_SECTOR_SIZE) == 0, "length %u not a multiple of bio sector size", len);

	/* First of all, lock the corresponding queue */
	unsigned int bucket_num = block % BIO_BUCKET_SIZE;
bio_restart:
	spinlock_lock(&spl_bucket[bucket_num]);

	/* See if we can find the block in the bucket queue; if so, we can just return it */
	for (struct BIO* bio = bio_bucket[bucket_num]; bio != NULL; bio = bio->bucket_next) {
		if (bio->device != dev || bio->block != block)
			continue;

		/*
	 	 * This is the block we need; we must move it in front of the used queue to
		 * prevent it from being nuked.
		 */
		spinlock_lock(&spl_bio_lists);
		KASSERT(bio_usedlist != NULL, "usedlist is NULL");
		if (bio_usedlist != bio) {
			/* Remove ourselves from the chain... */
			if(bio->chain_prev != NULL)
				bio->chain_prev->chain_next = bio->chain_next;
			if(bio->chain_next != NULL)
				bio->chain_next->chain_prev = bio->chain_prev;
			/* ...and prepend us at the beginning */
			bio->chain_next = bio_usedlist;
			bio->chain_prev = bio_usedlist->chain_prev;
			bio_usedlist->chain_prev = bio;
			bio_usedlist = bio;
		}
		spinlock_unlock(&spl_bio_lists);

		spinlock_unlock(&spl_bucket[bucket_num]);
		KASSERT(bio->length == len, "bio item found with length %u, requested length %u", bio->length, len); /* XXX should avoid... somehow */
		return bio;
	}
	
	/* Grab a bio from the head of the queue */
	spinlock_lock(&spl_bio_lists);
	struct BIO* bio = bio_freelist;
	if (bio_freelist != NULL) {
		bio_freelist = bio_freelist->chain_next;
		bio_freelist->chain_prev = bio->chain_prev;
	}

	if (bio == NULL) {
		/* No bio's available; clean up some */
		spinlock_unlock(&spl_bio_lists);
		spinlock_unlock(&spl_bucket[bucket_num]);
		bio_cleanup();
		goto bio_restart;
	}

	/* And add it to the used list */
	if (bio_usedlist != NULL) {
		bio->chain_prev = bio_usedlist->chain_prev;
		bio_usedlist->chain_prev = bio;
	} else {
		bio->chain_prev = bio;
	}
	bio->chain_next = bio_usedlist;
	bio_usedlist = bio;
	spinlock_unlock(&spl_bio_lists);

	/* Find available data blocks in the bio data pool */
bio_restartdata:
	spinlock_lock(&spl_bio_bitmap);
	unsigned int chain_length = 0, cur_data_block = 0;
	unsigned int num_blocks = len / BIO_SECTOR_SIZE;
	for (; cur_data_block < bio_bitmap_size * 8; cur_data_block++) {
		if((bio_bitmap[cur_data_block / 8] & (1 << (cur_data_block % 8))) != 0) {
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
		spinlock_unlock(&spl_bio_bitmap);
		spinlock_unlock(&spl_bucket[bucket_num]);
		bio_cleanup();
		goto bio_restartdata;
	}
	/* Walk back the number of blocks we need to allocate, we know they are free */
	cur_data_block -= chain_length;

	/* Mark the blocks as in use */
	for (unsigned int n = cur_data_block; n < cur_data_block + (len / BIO_SECTOR_SIZE); n++) {
		bio_bitmap[n / 8] |= (1 << (n % 8));
	}
	spinlock_unlock(&spl_bio_bitmap);

	/* Hook the request to the corresponding bucket */
	bio->bucket_next = bio_bucket[bucket_num];
	if (bio_bucket[bucket_num] != NULL)
		bio_bucket[bucket_num]->bucket_prev = bio;
	bio_bucket[bucket_num] = bio;
	spinlock_unlock(&spl_bucket[bucket_num]);

	/*
	 * Throw away any flags the buffer has (as this is a new request,
	 * we can't anything more sensible yet)
	 */
	bio->flags = BIO_FLAG_DIRTY;
	bio->device = dev;
	bio->block = block;
	bio->length = len;
	bio->data = bio_data + (cur_data_block * BIO_SECTOR_SIZE);
	return bio;
}

/*
 * Locates a given BIO buffer. Note that this function gives a race between allocate and use XXX
 */
static struct BIO*
bio_find(device_t dev, block_t block, size_t len)
{
	unsigned int bucket_num = block % BIO_BUCKET_SIZE;

	spinlock_lock(&spl_bucket[bucket_num]);
	for (struct BIO* bio = bio_bucket[bucket_num]; bio != NULL; bio = bio->bucket_next) {
		if (bio->device != dev)
			continue;

		KASSERT(bio->length == len, "bio item found with length %u, requested length %u", bio->length, len); /* XXX should avoid... somehow */
		spinlock_unlock(&spl_bucket[bucket_num]);
		return bio;
	}
	spinlock_unlock(&spl_bucket[bucket_num]);

	return NULL;
}

/*
 * Called by BIO consumers when they are done with a bio buffer.
 */
void
bio_free(struct BIO* bio)
{
}

int
bio_waitcomplete(struct BIO* bio)
{	
	while((bio->flags & BIO_FLAG_DIRTY) != 0) {
		reschedule();
	}
}

struct BIO*
bio_read(device_t dev, block_t block, size_t len)
{
	struct BIO* bio = bio_get(dev, block, len);
	/*
	 * If we have a block that is not dirty, this means we can return it. XXX
	 * what about dirty write?
	 */
	if ((bio->flags & BIO_FLAG_DIRTY) == 0)
		return bio;
	bio->flags |= BIO_FLAG_READ;

	/* kick the device; we want it to read */
	device_read(dev, (void*)bio, len, (off_t)block);

	/* ... and wait until we have something to report... */
	bio_waitcomplete(bio);
	return bio;
}

void
bio_set_error(struct BIO* bio)
{
	bio->flags = (bio->flags & ~BIO_FLAG_DIRTY) | BIO_FLAG_ERROR;
}

void
bio_set_ready(struct BIO* bio)
{
	bio->flags &= ~BIO_FLAG_DIRTY;
}

void
bio_dump()
{
	kprintf("bio dump\n");

	unsigned int freelist_avail = 0, usedlist_used = 0;
	spinlock_lock(&spl_bio_lists);
	struct BIO* prev_freelist;
	for (struct BIO* bio = bio_freelist; bio != NULL; bio = bio->chain_next) {
		freelist_avail++; prev_freelist = bio;
	}
	if (bio_freelist != NULL)
		KASSERT(bio_freelist->chain_prev == prev_freelist, "freelist head/tail not in sync (%p != %p)", bio_freelist->chain_prev, prev_freelist);
	struct BIO* prev_usedlist;
	for (struct BIO* bio = bio_usedlist; bio != NULL; bio = bio->chain_next) {
		usedlist_used++; prev_usedlist = bio;
	}
	if (bio_usedlist != NULL) {
		KASSERT(bio_usedlist->chain_prev == prev_usedlist, "usedlist head/tail not in sync (%p != %p)", bio_usedlist->chain_prev, prev_usedlist);
	}
	spinlock_unlock(&spl_bio_lists);
	kprintf("lists: %u bio's used, %u bio's free, %u total\n", usedlist_used, freelist_avail, BIO_NUM_BUFFERS);
	KASSERT(freelist_avail + usedlist_used == BIO_NUM_BUFFERS, "chain length does not add up");

	kprintf("buckets:");
	for (unsigned int bucket_num = 0; bucket_num < BIO_BUCKET_SIZE; bucket_num++) {
		kprintf("%u =>", bucket_num);
		spinlock_lock(&spl_bucket[bucket_num]);
		unsigned int n = 0;
		for (struct BIO* bio = bio_bucket[bucket_num]; bio != NULL; bio = bio->bucket_next, n++) {
			kprintf(" 0x%p (%u)", bio, bio->block);
		}
		if (n == 0)
			kprintf(" (empty)");
		kprintf("\n");
		spinlock_unlock(&spl_bucket[bucket_num]);
	}

	unsigned int databuf_avail = 0;
	kprintf("data buffers:");
	spinlock_lock(&spl_bio_bitmap);
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
	spinlock_unlock(&spl_bio_bitmap);
	kprintf(" %u available (%u total)\n", databuf_avail, BIO_DATA_SIZE / BIO_SECTOR_SIZE);

	KASSERT(databuf_avail <= (BIO_DATA_SIZE / BIO_SECTOR_SIZE), "more bio data available than total");
}

/* vim:set ts=2 sw=2: */
