#include <machine/io.h>
#include <ananas/dev/ata.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <mbr.h>

struct ATADISK_PRIVDATA {
	int unit;
	uint64_t size;	/* in sectors */
};

static int
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
		return 0;
	}
	mbr_process(dev, bio);
	bio_free(bio);
	return 1;
}

static ssize_t
atadisk_read(device_t dev, void* buffer, size_t length, off_t offset)
{
	struct ATADISK_PRIVDATA* priv = (struct ATADISK_PRIVDATA*)dev->privdata;
	struct ATA_REQUEST_ITEM item;
	KASSERT(length > 0, "invalid length");
	KASSERT(length % 512 == 0, "invalid length"); /* XXX */
	KASSERT(buffer != NULL, "invalid buffer");

	/* XXX boundary check */
	item.unit = priv->unit;
	item.lba = offset;
	item.count = length / 512;
	item.bio = (struct BIO*)buffer;
	item.command = ATA_CMD_READ_SECTORS;
	dev->parent->driver->drv_enqueue(dev->parent, &item);
	dev->parent->driver->drv_start(dev->parent);
}

struct DRIVER drv_atadisk = {
	.name					= "atadisk",
	.drv_attach		= atadisk_attach,
	.drv_read			= atadisk_read
};

/* vim:set ts=2 sw=2: */
