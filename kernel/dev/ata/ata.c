#include <machine/io.h>
#include <sys/dev/ata.h>
#include <sys/bio.h>
#include <sys/device.h>
#include <sys/irq.h>
#include <sys/mm.h>
#include <sys/lib.h>

#define ATA_GET_WORD(x) \
	((uint16_t)(x)[0] << 8 | (x)[1])
#define ATA_GET_DWORD(x) \
	((uint32_t)(ATA_GET_WORD(x+2) << 16) | \
	((uint32_t)(ATA_GET_WORD(x))))
#define ATA_GET_QWORD(x) \
	((uint64_t)(ATA_GET_DWORD(x+4) << 32) | \
	((uint64_t)(ATA_GET_DWORD(x))))

struct ATA_PRIVDATA {
	uint32_t	io_port;
};

struct BIO* ata_currentbio = NULL;

extern struct DRIVER drv_atadisk;
	
#define SECTOR_SIZE 512 /* XXX */

int md_interrupts_enabled(); /* XXX */

void
ata_irq(device_t dev)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;
	/* If we were not doing a request, no point in continuing */
	if (ata_currentbio == NULL)
		return;

	int stat = inb(priv->io_port + ATA_PIO_STATCMD);
	if (stat & ATA_PIO_STATUS_ERR) {
		ata_currentbio->flags |= BIO_FLAG_ERROR;
		kprintf("ata error %u!\n", priv->io_port);
	} else {
		/*
		 * Request OK - fill the bio data XXX need to port 'rep insw'. We do this
		 * before updating the buffer status to prevent 
		 */
		for(int count = 0; count < ata_currentbio->length; ) {
			uint16_t data = inw(priv->io_port + ATA_PIO_DATA);
			ata_currentbio->data[count++] = data & 0xff;
			ata_currentbio->data[count++] = data >> 8;
		}
	}

	/* Current request is done. Sign it off and away it goes */
	ata_currentbio->flags |= BIO_FLAG_DONE;
	ata_currentbio = NULL;
}

static int
ata_read_sectors(device_t dev, int slave, uint32_t lba, uint32_t count)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;

	outb(priv->io_port + ATA_PIO_DRIVEHEAD, 0xe0 | (slave ? 0x10 : 0) | ((lba >> 24) & 0xf));
	/*outb(priv->io_port + ATA_PIO_ERROR, 0);*/
	outb(priv->io_port + ATA_PIO_COUNT, count);
	outb(priv->io_port + ATA_PIO_LBA_1, lba & 0xff);
	outb(priv->io_port + ATA_PIO_LBA_2, (lba >> 8) & 0xff);
	outb(priv->io_port + ATA_PIO_LBA_3, (lba >> 16) & 0xff);
	outb(priv->io_port + ATA_PIO_STATCMD, ATA_CMD_READ_SECTORS);

	return 1;
}

static int
ata_identify(device_t dev, int slave)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;

	outb(priv->io_port + ATA_PIO_DRIVEHEAD, 0xa0 | (slave ? 0x10 : 0));
	outb(priv->io_port + ATA_PIO_STATCMD, ATA_CMD_IDENTIFY);
	int x = inb(priv->io_port + ATA_PIO_STATCMD);
	if (x == 0)
		return 0;

	/* Wait until ERR is turned on, or !BSY && DRQ */
	while (1) {
		x = inb(priv->io_port  + ATA_PIO_STATCMD);
		if (!(x & ATA_PIO_STATUS_BSY) && (x & ATA_PIO_STATUS_DRQ))
			break;
		if (x & ATA_PIO_STATUS_ERR)
			return 0;
	}

	/* Grab the result of the identification command */
	char buf[sizeof(struct ATA_IDENTIFY)];
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)buf;
	int count = 0 ;
	while (count < SECTOR_SIZE) {
		x = inw(priv->io_port + ATA_PIO_DATA);
		buf[count++] = x >> 8;
		buf[count++] = x & 0xff; 
	}

	/* Calculate the length of the disk */
	unsigned long size = ATA_GET_DWORD(identify->lba_sectors);
	if (ATA_GET_WORD(identify->features2) & ATA_FEAT2_LBA48) {
		size  = ATA_GET_QWORD(identify->lba48_sectors);
	}

	/* Chop off the trailing spaces off the identity */
	for (int i = sizeof(identify->model) - 1; i > 0; i--) {
		if (identify->model[i] != ' ')
			break;
		identify->model[i] = '\0';
	}

	kprintf("model [%s] - foo\n", identify->model);
	kprintf("size: %u sectors (%u MB)\n", size, size / ((1024UL * 1024UL) / 512UL));

	/* Add the disk to our bus */
	device_t new_dev = device_alloc(dev, &drv_atadisk);
	device_set_biodev(new_dev, dev);	/* force I/O to go through us */
	device_add_resource(new_dev, RESTYPE_CHILDNUM, 0 /* master */, 0);
	device_attach_single(new_dev);
	device_print_attachment(new_dev);
	return 1;
}

static int
ata_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return 1; /* XXX */

	struct ATA_PRIVDATA* priv = kmalloc(sizeof(struct ATA_PRIVDATA));
	priv->io_port = (uint32_t)(uintptr_t)res_io;
	dev->privdata = priv;

	/* Ensure there's something living at the I/O addresses */
	if (inb(priv->io_port + ATA_PIO_STATCMD) == 0xff) return 1;

	if (!irq_register((uintptr_t)res_irq, dev, ata_irq))
		return 1;

	ata_identify(dev, 0);
	return 0;
}

static void
ata_start(device_t dev)
{
	/* XXX we only do a single request at a time */
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->biodev->privdata;
	struct BIO* bio = bio_get_next(dev);
	KASSERT(bio != NULL, "ata_start without pending requests");

	KASSERT(BIO_IS_INUSE(bio), "pending bio isn't in use");
	KASSERT(!BIO_IS_DONE(bio), "pending bio already completed");
	if (BIO_IS_WRITE(bio)) {
		kprintf("ata_start(): XXX no writes yet\n");
		bio->flags |= BIO_FLAG_DONE | BIO_FLAG_ERROR;
		return;
	}

	/* XXX lock ?? */
	ata_currentbio = bio;

	/* feed it to the disk */
	ata_read_sectors(dev->biodev, 0 /* XXX slave */, bio->block, bio->length / 512);

	/*
	 * We may get called while interrupts are disabled. If this is the case, wait for
	 * completion here by polling.
	 */
	if (md_interrupts_enabled())
		return;

	/* Handle polling of the status */
	//kprintf("ata_start(): doing polling\n");
	int x;
	while (1) {
		x = inb(priv->io_port + ATA_PIO_STATCMD);
		if (!(x & ATA_PIO_STATUS_BSY) && (x & ATA_PIO_STATUS_DRQ))
			break;
		if (x & ATA_PIO_STATUS_ERR)
			break;
	}
	ata_irq(dev->biodev);
}

struct DRIVER drv_ata = {
	.name					= "ata",
	.drv_probe		= NULL,
	.drv_attach		= ata_attach,
	.drv_start		= ata_start
};

DRIVER_PROBE(ata)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
