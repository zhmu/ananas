#include <ananas/types.h>
#include <ananas/dev/ata.h>
#include <ananas/dev/sata.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/endian.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <mbr.h>

TRACE_SETUP;

struct SATADISK_PRIVDATA {
	struct ATA_IDENTIFY sd_identify;
	uint64_t sd_size;	/* in sectors */
	uint32_t sd_flags;
#define SATADISK_FLAGS_LBA48 1
};

static errorcode_t
satadisk_attach(device_t dev)
{
	struct SATADISK_PRIVDATA* priv = kmalloc(sizeof *priv);
	memset(priv, 0, sizeof(*priv));
	dev->privdata = priv;

	/*
	 * Once we are here, we know we're attaching to a SATA disk-device; we don't
	 * know anything else though, so we'll have to issue an IDENTIFY command to
	 * get down to the details.
	 */
	semaphore_t sem;
	sem_init(&sem, 0);

	struct SATA_REQUEST sr;
	memset(&sr, 0, sizeof(sr));
	sata_fis_h2d_make_cmd(&sr.sr_fis.fis_h2d, ATA_CMD_IDENTIFY);
	sr.sr_fis_length = 20;
	sr.sr_semaphore = &sem;
	sr.sr_count = 512;
	sr.sr_buffer = &priv->sd_identify;
	sr.sr_flags = SATA_REQUEST_FLAG_READ;
	dev->parent->driver->drv_enqueue(dev->parent, &sr);
	dev->parent->driver->drv_start(dev->parent);

	/* Wait until the request has been completed */
	sem_wait(&sem);

	/* Fix endianness */
	uint16_t* p = (void*)&priv->sd_identify;
	for (unsigned int n = 0; n < sizeof(priv->sd_identify) / 2; n++, p++)
		*p = betoh16(*p);

	/* Calculate the length of the disk */
	priv->sd_size = ATA_GET_DWORD(priv->sd_identify.lba_sectors);
	if (ATA_GET_WORD(priv->sd_identify.features2) & ATA_FEAT2_LBA48) {
		priv->sd_size  = ATA_GET_QWORD(priv->sd_identify.lba48_sectors);
		priv->sd_flags |= SATADISK_FLAGS_LBA48;
	}

	/* Terminate the model name */
	for(int n = sizeof(priv->sd_identify.model) - 1; n > 0 && priv->sd_identify.model[n] == ' '; n--)
		priv->sd_identify.model[n] = '\0';

	device_printf(dev, "<%s> - %u MB",
	 priv->sd_identify.model,
 	 priv->sd_size / ((1024UL * 1024UL) / 512UL));

	/*
	 * Read the first sector and pass it to the MBR code; this is crude
	 * and does not really belong here.
	 */
	struct BIO* bio = bio_read(dev, 0, BIO_SECTOR_SIZE);
	if (BIO_IS_ERROR(bio)) {
		kfree(priv);
		return ANANAS_ERROR(IO); /* XXX should get error from bio */
	}

	mbr_process(dev, bio);
	bio_free(bio);
	return ANANAS_ERROR(IO);
}

static errorcode_t
satadisk_bread(device_t dev, struct BIO* bio)
{
	KASSERT(bio != NULL, "invalid buffer");
	KASSERT(bio->length > 0, "invalid length");
	KASSERT(bio->length % 512 == 0, "invalid length"); /* XXX */

	struct SATA_REQUEST sr;
	memset(&sr, 0, sizeof(sr));
	/* XXX  we shouldn't always use lba-48 */
	sata_fis_h2d_make_cmd_lba48(&sr.sr_fis.fis_h2d, ATA_CMD_DMA_READ_EXT, bio->io_block, bio->length / BIO_SECTOR_SIZE);
	sr.sr_fis_length = 20;
	sr.sr_count = bio->length;
	sr.sr_bio = bio;
	sr.sr_flags = SATA_REQUEST_FLAG_READ;
	dev->parent->driver->drv_enqueue(dev->parent, &sr);
	dev->parent->driver->drv_start(dev->parent);
	return ANANAS_ERROR_OK;
}

static errorcode_t
satadisk_bwrite(device_t dev, struct BIO* bio)
{
#if 0
	struct SATA_REQUEST sr;
	memset(&sr, 0, sizeof(sr));
	/* XXX  we shouldn't always use lba-47 */
	sata_fis_h2d_make_cmd_lba48(&sr.sr_fis.fis_h2d, ATA_CMD_DMA_READ_EXT, bio->io_block, bio->length);
	sr.sr_fis_length = 20;
	sr.sr_count = bio->length;
	sr.sr_bio = bio;
	sr.sr_flags = SATA_REQUEST_FLAG_READ;
	dev->parent->driver->drv_enqueue(dev->parent, &sr);
	dev->parent->driver->drv_start(dev->parent);
#endif
	kprintf("satadisk write todo\n");
	return ANANAS_ERROR(IO);
}

struct DRIVER drv_sata_disk = {
	.name					= "satadisk",
	.drv_attach		= satadisk_attach,
	.drv_bread		= satadisk_bread,
	.drv_bwrite   = satadisk_bwrite
};

/* vim:set ts=2 sw=2: */
