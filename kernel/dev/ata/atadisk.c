#include <machine/io.h>
#include <sys/dev/ata.h>
#include <sys/bio.h>
#include <sys/lib.h>
#include <mbr.h>

static int
atadisk_attach(device_t dev)
{
	void* res = device_alloc_resource(dev, RESTYPE_CHILDNUM, 0);
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)dev->privdata;

	/* Calculate the length of the disk */
	unsigned long size = ATA_GET_DWORD(identify->lba_sectors);
	if (ATA_GET_WORD(identify->features2) & ATA_FEAT2_LBA48) {
		size  = ATA_GET_QWORD(identify->lba48_sectors);
	}

	device_printf(dev, "<%s> - %u MB",
	 identify->model,
 	 size / ((1024UL * 1024UL) / 512UL));


	struct BIO* bio = bio_read(dev, 0, 512);
	if (BIO_IS_ERROR(bio))
		return 0;

	/* XXX certainly does not belong here */
	mbr_process(dev, bio);
	return 1;
}

struct DRIVER drv_atadisk = {
	.name					= "atadisk",
	.drv_attach		= atadisk_attach
};

/* vim:set ts=2 sw=2: */
