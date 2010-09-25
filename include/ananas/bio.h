#include <ananas/types.h>
#include <ananas/device.h>
#include <machine/param.h>	/* for PAGE_SIZE */

#ifndef __ANANAS_BIO_H__
#define __ANANAS_BIO_H__

/* Number of buckets used to hash a block number to */
#define BIO_BUCKET_SIZE		16

/* Number of BIO buffers available */
#define BIO_NUM_BUFFERS		1024

/* Amount of data available for BIO data, in bytes */
#define BIO_DATA_SIZE		(512 * 1024)

/* Size of a sector; any BIO block must be a multiple of this */
#define BIO_SECTOR_SIZE		512

#define BIO_IS_DIRTY(bio)	((bio)->flags & BIO_FLAG_DIRTY)
#define BIO_IS_READ(bio)	((bio)->flags & BIO_FLAG_READ)
#define BIO_IS_WRITE(bio)	((bio)->flags & BIO_FLAG_WRITE)
#define BIO_IS_ERROR(bio)	((bio)->flags & BIO_FLAG_ERROR)
#define BIO_DATA(bio)		((bio)->data)

/*
 * A basic I/O buffer, the root of all I/O requests. 
 */
struct BIO {
	uint32_t	flags;
#define BIO_FLAG_DIRTY	0x0001		/* I/O needs to be read/written */
#define BIO_FLAG_READ	0x0002		/* Must read data */
#define BIO_FLAG_WRITE	0x0004		/* Must write data */
#define BIO_FLAG_ERROR	0x8000		/* Request failed */
	device_t	device;		/* Device I/O'ing from */
	block_t		block;		/* Block number to I/O */
	unsigned int	length;		/* Length in bytes (<= PAGE_SIZE, so int will do) */
	void*		data;		/* Pointer to BIO data */

	struct BIO*	chain_prev;	/* Previous BIO in chain (free/used list)*/
	struct BIO*	chain_next;	/* Next BIO in chain (free/used list) */
	struct BIO*	bucket_prev;	/* Previous BIO in bucket */
	struct BIO*	bucket_next;	/* Next BIO in bucket */
};

void bio_init();
int bio_waitcomplete(struct BIO* bio);
void bio_set_error(struct BIO* bio);
void bio_set_done(struct BIO* bio);
struct BIO* bio_read(device_t dev, block_t block, size_t len);
struct BIO* bio_get_next(device_t dev);
void bio_free(struct BIO* bio);

#endif /* __ANANAS_BIO_H__ */
