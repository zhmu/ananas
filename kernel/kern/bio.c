#include <sys/mm.h>
#include <sys/bio.h>
#include <sys/lib.h>
#include <sys/schedule.h>

#define SECTOR_SIZE	 512	/* XXX */
#define BIO_NUM_BUFS 128

static struct BIO* bio_buffer;

void
bio_init()
{
	bio_buffer = kmalloc(BIO_NUM_BUFS * sizeof(struct BIO));
	memset(bio_buffer, 0, BIO_NUM_BUFS * sizeof(struct BIO));
	kprintf("bio: allocated %u KB for buffers\n", (BIO_NUM_BUFS * sizeof(struct BIO)) / 1024);
}

/*
 * Allocate a new bio buffer. This must always return a valid bio and wait for
 * one if needed.
 */
struct BIO*
bio_new()
{
	/*
	 * XXX yes, we should implement a freelist some day. oh, and locking. yeah...
	 */
	for (unsigned int i = 0; i < BIO_NUM_BUFS; i++) {
		if (bio_buffer[i].flags & BIO_FLAG_INUSE)
			continue;
		/* XXX race */

		/*
		 * Throw away any flags the buffer has (as this is a new request,
		 * we can't anything more sensible yet)
		 */
		bio_buffer[i].flags = BIO_FLAG_INUSE;
		return &bio_buffer[i];
	}

	/* XXX we should wait until a bio is available */
	panic("bio_new(): out of bio's!");
}

void
bio_free(struct BIO* bio)
{
	KASSERT(bio->flags & BIO_FLAG_INUSE, "bio is not in use");
	bio->flags &= ~BIO_FLAG_INUSE;
}

/*
 * Grabs the next bio to handle for a given device, or NULL if there is none.
 * XXX we should use per-device lists etc.
 */
struct BIO*
bio_get_next(device_t dev)
{
	/* XXX locking */
	for (unsigned int i = 0; i < BIO_NUM_BUFS; i++) {
		struct BIO* bio = &bio_buffer[i];
		if ((bio->flags & BIO_FLAG_INUSE) && !(bio->flags & BIO_FLAG_DONE) && bio->device == dev)
			return bio;
	}
	return NULL;
}

struct BIO*
bio_alloc(device_t dev, block_t block, size_t len)
{
	KASSERT(len <= BIO_BUF_DATASIZE, "size out of range");

	struct BIO* bio = bio_new();
	bio->device = dev;
	bio->block = block;
	bio->length = len;
	return bio;
}

int
bio_waitcomplete(struct BIO* bio)
{	
	KASSERT(bio->flags & BIO_FLAG_INUSE, "bio is not in use");

	while(!(bio->flags & BIO_FLAG_DONE)) {
		reschedule();
	}
}

struct BIO*
bio_read(device_t dev, block_t block, size_t len)
{
	struct BIO* bio = bio_alloc(dev, block, len);
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
	KASSERT(bio->flags & BIO_FLAG_INUSE, "bio is not in use");
	bio->flags |= BIO_FLAG_DONE | BIO_FLAG_ERROR;
}

void
bio_set_done(struct BIO* bio)
{
	KASSERT(bio->flags & BIO_FLAG_INUSE, "bio is not in use");
	bio->flags |= BIO_FLAG_DONE;
}

/* vim:set ts=2 sw=2: */
