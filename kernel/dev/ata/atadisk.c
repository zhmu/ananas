#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/dev/ata.h>
#include <ananas/x86/io.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <mbr.h>

TRACE_SETUP;

struct ATADISK_PRIVDATA {
	int unit;
	uint64_t size;	/* in sectors */
};

static errorcode_t
atadisk_attach(device_t dev)
{
	int unit = (int)device_alloc_resource(dev, RESTYPE_CHILDNUM, 0);
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)dev->privdata;

	/* Allocate our private data */
	struct ATADISK_PRIVDATA* priv = kmalloc(sizeof(struct ATADISK_PRIVDATA));
	dev->privdata = priv;
	priv->unit = unit;

	/* Calculate the length of the disk */
	priv->size = ATA_GET_DWORD(identify->lba_sectors);
	if (ATA_GET_WORD(identify->features2) & ATA_FEAT2_LBA48) {
		priv->size  = ATA_GET_QWORD(identify->lba48_sectors);
	}

	device_printf(dev, "<%s> - %u MB",
	 identify->model,
 	 priv->size / ((1024UL * 1024UL) / 512UL));

	/*
	 * Read the first sector and pass it to the MBR code; this is crude
	 * and does not really belong here.
	 */
	struct BIO* bio = bio_read(dev, 0, 512);
	if (BIO_IS_ERROR(bio)) {
		kfree(priv);
		return ANANAS_ERROR(IO); /* XXX should get error from bio */
	}
	mbr_process(dev, bio);
	bio_free(bio);
	return 1;
}

static errorcode_t
atadisk_bread(device_t dev, struct BIO* bio)
{
	struct ATADISK_PRIVDATA* priv = (struct ATADISK_PRIVDATA*)dev->privdata;
	struct ATA_REQUEST_ITEM item;
	KASSERT(bio != NULL, "invalid buffer");
	KASSERT(bio->length > 0, "invalid length");
	KASSERT(bio->length % 512 == 0, "invalid length"); /* XXX */

	/* XXX boundary check */
	item.unit = priv->unit;
	item.lba = bio->io_block;
	item.count = bio->length / 512;
	item.bio = bio;
	item.command = ATA_CMD_READ_SECTORS;
	dev->parent->driver->drv_enqueue(dev->parent, &item);
	dev->parent->driver->drv_start(dev->parent);
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_atadisk = {
	.name					= "atadisk",
	.drv_attach		= atadisk_attach,
	.drv_bread		= atadisk_bread
};

/* vim:set ts=2 sw=2: */
