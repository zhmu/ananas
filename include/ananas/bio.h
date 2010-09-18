#include <ananas/types.h>
#include <ananas/device.h>
#include <machine/param.h>	/* for PAGE_SIZE */

#ifndef __BIO_H__
#define __BIO_H__

/* XXX */
#define BIO_BUF_DATASIZE	PAGE_SIZE

#define BIO_IS_INUSE(bio)	((bio)->flags & BIO_FLAG_INUSE)
#define BIO_IS_DONE(bio)	((bio)->flags & BIO_FLAG_DONE)
#define BIO_IS_READ(bio)	((bio)->flags & BIO_FLAG_READ)
#define BIO_IS_WRITE(bio)	((bio)->flags & BIO_FLAG_WRITE)
#define BIO_IS_ERROR(bio)	((bio)->flags & BIO_FLAG_ERROR)
#define BIO_DATA(bio)		((bio)->data)

/*
 * A basic BIO buffer, the root of all I/O requests. 
 */
struct BIO {
	uint32_t	flags;
#define BIO_FLAG_INUSE	0x0001		/* I/O is in use */
#define BIO_FLAG_DONE	0x0002		/* Request completed */
#define BIO_FLAG_READ	0x0004		/* Must read data */
#define BIO_FLAG_WRITE	0x0008		/* Must write data */
#define BIO_FLAG_ERROR	0x8000		/* Request failed */
	device_t	device;		/* Device I/O'ing from */
	block_t		block;		/* Block number to I/O */
	size_t		length;		/* Length in bytes */
	uint8_t		data[BIO_BUF_DATASIZE];
};

void bio_init();
int bio_waitcomplete(struct BIO* bio);
void bio_set_error(struct BIO* bio);
void bio_set_done(struct BIO* bio);
struct BIO* bio_read(device_t dev, block_t block, size_t len);
struct BIO* bio_get_next(device_t dev);
void bio_free(struct BIO* bio);

#endif /* __BIO_H__ */
