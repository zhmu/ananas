#include <machine/io.h>
#include <sys/dev/ata.h>
#include <sys/bio.h>
#include <sys/device.h>
#include <sys/irq.h>
#include <sys/mm.h>
#include <sys/lib.h>

extern struct DRIVER drv_atadisk;
extern struct DRIVER drv_atacd;

struct ATA_PRIVDATA {
	uint32_t	io_port;
	struct ATA_REQUEST_QUEUE requests;
};

void
ata_irq(device_t dev)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;

	/* If we were not doing a request, no point in continuing */
	if (QUEUE_EMPTY(&priv->requests)) {
		return;
	}

	/*
	 * Fetch the request and remove it from the queue; ATA may give extra interrupts, which we
	 * happily ignore as the queue is empty when they arrive.
	 */
	struct ATA_REQUEST_ITEM* item = QUEUE_HEAD(&priv->requests, struct ATA_REQUEST_ITEM);
	QUEUE_POP_HEAD(&priv->requests, struct ATA_REQUEST_ITEM);

	/* If this is an ATAPI command, we may need to send the command bytes at this point */
	if (item->command == ATA_CMD_PACKET) {
		/*
		 * In ATAPI-land, we obtain the number of bytes that could actually be read - this much
		 * data is waiting for us.
		 */
		uint16_t len = (uint16_t)inb(priv->io_port + ATA_PIO_LBA_3) << 8 | 
		                         inb(priv->io_port + ATA_PIO_LBA_2);
		item->bio->length = len;
	}

	int stat = inb(priv->io_port + ATA_PIO_STATCMD);
	if (stat & ATA_PIO_STATUS_ERR) {
		kprintf("ata error %u!\n", priv->io_port);
		bio_set_error(item->bio);
		return;
	}
	/*
	 * Request OK - fill the bio data XXX need to port 'rep insw'. We do this
	 * before updating the buffer status to prevent 
	 */
	for(int count = 0; count < item->bio->length; ) {
		uint16_t data = inw(priv->io_port + ATA_PIO_DATA);
		item->bio->data[count++] = data & 0xff;
		item->bio->data[count++] = data >> 8;
	}
	
	/* Current request is done. Sign it off and away it goes */
	item->bio->flags |= BIO_FLAG_DONE;
}

/*
 * This handles identification of a master/slave device on a given ata bus. It
 * will attempt both the ATA IDENTIFY and ATAPI IDENTIFY commands.
 *
 * This function returns 0 on failure or the identify command code that worked
 * on success.
 */
static int
ata_identify(device_t dev, int unit, struct ATA_IDENTIFY* identify)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;
	uint8_t identify_cmds[] = { ATA_CMD_IDENTIFY, ATA_CMD_IDENTIFY_PACKET, 0 };

	for (int cur_cmd = 0; identify_cmds[cur_cmd] != 0; cur_cmd++) {
		outb(priv->io_port + ATA_PIO_DRIVEHEAD, 0xa0 | (unit ? 0x10 : 0));
		outb(priv->io_port + ATA_PIO_STATCMD, identify_cmds[cur_cmd]);
		int x = inb(priv->io_port + ATA_PIO_STATCMD);
		if (x == 0)
			continue;

		/* Wait until ERR is turned on, or !BSY && DRQ */
		while (1) {
			x = inb(priv->io_port  + ATA_PIO_STATCMD);
			if (!(x & ATA_PIO_STATUS_BSY) && (x & ATA_PIO_STATUS_DRQ))
				break;
			if (x & ATA_PIO_STATUS_ERR)
				break;
		}
		if (x & ATA_PIO_STATUS_ERR)
			continue;

		/* Grab the result of the identification command XXX implement insw() */
		char* buf = (char*)identify;
		for (int count = 0; count < SECTOR_SIZE; ) {
			x = inw(priv->io_port + ATA_PIO_DATA);
			buf[count++] = x >> 8;
			buf[count++] = x & 0xff; 
		}

		/* Chop off the trailing spaces off the identity */
		for (int i = sizeof(identify->model) - 1; i > 0; i--) {
			if (identify->model[i] != ' ')
				break;
			identify->model[i] = '\0';
		}

		return identify_cmds[cur_cmd];
	}

	return 0;
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
	QUEUE_INIT(&priv->requests);
	dev->privdata = priv;

	/* Ensure there's something living at the I/O addresses */
	if (inb(priv->io_port + ATA_PIO_STATCMD) == 0xff) return 1;

	if (!irq_register((uintptr_t)res_irq, dev, ata_irq))
		return 1;

	/* reset the control register - this ensures we receive interrupts */
	outb(priv->io_port + ATA_PIO_DCR, 0);
	return 0;
}

#define ATA_DELAY() \
		inb(priv->io_port + 0x206); inb(priv->io_port + 0x206);  \
		inb(priv->io_port + 0x206); inb(priv->io_port + 0x206); 

static void
ata_start(device_t dev)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;

	KASSERT(!QUEUE_EMPTY(&priv->requests), "ata_start() with empty queue");

	/* XXX locking */
	/* XXX only do a single item now */

	struct ATA_REQUEST_ITEM* item = QUEUE_HEAD(&priv->requests, struct ATA_REQUEST_ITEM);

	if (item->command != ATA_CMD_PACKET) {
		/* Feed the request to the drive - disk */
		outb(priv->io_port + ATA_PIO_DRIVEHEAD, 0xe0 | (item->unit ? 0x10 : 0) | ((item->lba >> 24) & 0xf));
		outb(priv->io_port + ATA_PIO_COUNT, item->count);
		outb(priv->io_port + ATA_PIO_LBA_1, item->lba & 0xff);
		outb(priv->io_port + ATA_PIO_LBA_2, (item->lba >> 8) & 0xff);
		outb(priv->io_port + ATA_PIO_LBA_3, (item->lba >> 16) & 0xff);
		outb(priv->io_port + ATA_PIO_STATCMD, item->command);
	} else {
		/* Feed the request to the device - ATAPI */
		outb(priv->io_port + ATA_PIO_DRIVEHEAD, item->unit << 4);
		ATA_DELAY(); ATA_DELAY();
		outb(priv->io_port + ATA_PIO_ERROR, 0); /* no DMA yet! */
		outb(priv->io_port + ATA_PIO_LBA_2, item->count & 0xff); /* note: in bytes! */
		outb(priv->io_port + ATA_PIO_LBA_3, item->count >> 8);
		outb(priv->io_port + ATA_PIO_STATCMD, item->command);

		/* Wait until the command is accepted */
		uint8_t status;
		while (inb(priv->io_port + ATA_PIO_STATCMD) & ATA_PIO_STATUS_BSY) ;
		while (1) {
			status = inb(priv->io_port + ATA_PIO_STATCMD);
			if (status & ATA_PIO_STATUS_ERR) {
				/* Got an error - this means the request cannot be completed */
				bio_set_error(item->bio);
;				return;
			}
			if (status & ATA_PIO_STATUS_DRQ)
				break;
		}
		for (unsigned int i = 0; i < 6; i++) {
			/* XXX We really need outsw() */
			outw(priv->io_port + ATA_PIO_DATA, *(uint16_t*)(&item->atapi_command[i * 2]));
		}
	}

	/*
	 * Now, we must wait for the IRQ to handle it. XXX what about errors?
 	 */
}

static void
ata_attach_children(device_t dev)
{
	struct ATA_IDENTIFY identify;

	for (int unit = 0; unit < 2; unit++) {
		int cmd = ata_identify(dev, unit, &identify);
		if (!cmd)
			continue;

		struct DRIVER* driver = NULL;
		if (cmd == ATA_CMD_IDENTIFY) {
			driver = &drv_atadisk;
		} else if (cmd == ATA_CMD_IDENTIFY_PACKET) {
			/*
			 * We found something that replied to ATAPI. First of all, do a sanity
			 * check to ensure it speaks valid ATAPI.
			 */
			uint16_t general_cfg = ATA_GET_WORD(identify.general_cfg);
			if ((general_cfg & ATA_GENCFG_NONATA) == 0 ||
					(general_cfg & ATA_GENCFG_NONATAPI) != 0)
				continue;

			/* Isolate the device type; this tells us which driver we need to use */
			uint8_t dev_type = (general_cfg >> 8) & 0x1f;
			switch (dev_type) {
				case ATA_TYPE_CDROM:
					driver = &drv_atacd;
					break;
				default:
					device_printf(dev, "detected unsupported device as unit %u, ignored", unit);
					continue;
			}
		}

		KASSERT(driver != NULL, "identified device, yet no driver");
		device_t new_dev = device_alloc(dev, driver);
		new_dev->privdata = (void*)&identify; /* XXX this is a hack; we should have an userpointer */
		device_add_resource(new_dev, RESTYPE_CHILDNUM, unit, 0);
		device_attach_single(new_dev);
	}
}

static void
ata_enqueue(device_t dev, struct ATA_REQUEST_ITEM* item)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;
	QUEUE_ADD_TAIL(&priv->requests, item, struct ATA_REQUEST_ITEM);
}

struct DRIVER drv_ata = {
	.name					= "ata",
	.drv_probe		= NULL,
	.drv_attach		= ata_attach,
	.drv_attach_children		= ata_attach_children,
	.drv_enqueue	= ata_enqueue,
	.drv_start		= ata_start
};

DRIVER_PROBE(ata)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
