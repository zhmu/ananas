#ifndef __ANANAS_BIO_H__
#define __ANANAS_BIO_H__

#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/list.h>
#include <machine/param.h>	/* for PAGE_SIZE */

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
	uint32_t  	flags;
#define BIO_FLAG_PENDING	0x0001	/* Block is pending read */
#define BIO_FLAG_DIRTY		0x0002	/* I/O needs to be written */
#define BIO_FLAG_ERROR		0x8000	/* Request failed */
	Ananas::Device* device;	/* Device I/O'ing from */
	blocknr_t	  block;	/* Block number to I/O */
	blocknr_t	  io_block;	/* Translated block number to I/O */
	unsigned int	  length;	/* Length in bytes (<= PAGE_SIZE, so int will do) */
	void*		  data;		/* Pointer to BIO data */
	semaphore_t       sem;          /* Semaphore for this BIO */

	LIST_FIELDS_IT(struct BIO, chain);	/* Chain queue */
	LIST_FIELDS_IT(struct BIO, bucket);	/* Bucket queue */
};

/* Flags of BIO_READ */
#define BIO_READ_NODATA		0x0001	/* Caller is not interested in the data */

void bio_set_error(struct BIO* bio);
void bio_set_available(struct BIO* bio);
void bio_set_dirty(struct BIO* bio);
struct BIO* bio_get(Ananas::Device* device, blocknr_t block, size_t len, int flags);

static inline struct BIO* bio_read(Ananas::Device* device, blocknr_t block, size_t len)
{
	return bio_get(device, block, len, 0);
}

struct BIO* bio_get_next(Ananas::Device* device);
void bio_free(struct BIO* bio);
void bio_dump();

#endif /* __ANANAS_BIO_H__ */
