#include <ananas/types.h>
#include <ananas/dev/ata.h>
#include <ananas/x86/io.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>

TRACE_SETUP;

extern struct DRIVER drv_atadisk;
extern struct DRIVER drv_atacd;

struct ATA_PRIVDATA {
	uint32_t	io_port;
	uint32_t	io_port2;
	struct ATA_REQUEST_QUEUE requests;
};

void
ata_irq(device_t dev)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;

	int stat = inb(priv->io_port + ATA_REG_STATUS);

	/* If we were not doing a request, no point in continuing */
	if (QUEUE_EMPTY(&priv->requests)) {
		return;
	}

	/*
	 * Fetch the request and remove it from the queue; ATA may give extra interrupts, which we
	 * happily ignore as the queue is empty when they arrive.
	 */
	struct ATA_REQUEST_ITEM* item = QUEUE_HEAD(&priv->requests);
	QUEUE_POP_HEAD(&priv->requests);
	KASSERT(item->bio != NULL, "ata queue item without associated bio buffer!");

	/* If this is an ATAPI command, we may need to send the command bytes at this point */
	if (item->command == ATA_CMD_PACKET) {
		/*
		 * In ATAPI-land, we obtain the number of bytes that could actually be read - this much
		 * data is waiting for us.
		 */
		uint16_t len = (uint16_t)inb(priv->io_port + ATA_REG_CYL_HI) << 8 | 
		                         inb(priv->io_port + ATA_REG_CYL_LO);
		item->bio->length = len;
	}

	if (stat & ATA_STAT_ERR) {
		kprintf("ata error %x ==> %x\n", stat,inb(priv->io_port + 1));
		bio_set_error(item->bio);
		kfree(item);
		return;
	}
	/*
	 * Request OK - fill the bio data XXX need to port 'rep insw'. We do this
	 * before updating the buffer status to prevent 
	 */
	uint8_t* bio_data = item->bio->data;
	for(int count = 0; count < item->bio->length / 2; count++) {
		uint16_t data = inw(priv->io_port + ATA_REG_DATA);
		*bio_data++ = data & 0xff;
		*bio_data++ = data >> 8;
	}
	
	/* Current request is done. Sign it off and away it goes */
	item->bio->flags &= ~BIO_FLAG_DIRTY;
	kfree(item);
}

static uint8_t
ata_read_status(device_t dev) {
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;
	return inb(priv->io_port2 + ATA_REG_ALTSTATUS);
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
	//uint8_t identify_cmds[] = { ATA_CMD_IDENTIFY, ATA_CMD_IDENTIFY_PACKET, 0 };
	uint8_t cmd = ATA_CMD_IDENTIFY;

#define TINY_WAIT() \
	do { \
		inb(priv->io_port2 + ATA_REG_ALTSTATUS); \
		inb(priv->io_port2 + ATA_REG_ALTSTATUS); \
		inb(priv->io_port2 + ATA_REG_ALTSTATUS); \
		inb(priv->io_port2 + ATA_REG_ALTSTATUS); \
	} while(0);

	/* Select drive */
	outb(priv->io_port + ATA_REG_DEVICEHEAD, 0xa0 | (unit << 4));
	TINY_WAIT();

	/* Perform a software reset */
	outb(priv->io_port2 + ATA_REG_DEVCONTROL, ATA_DCR_SRST);
	outb(priv->io_port2 + ATA_REG_DEVCONTROL, 0);
	TINY_WAIT();

	/*
	 * Select a device and ask it to identify itself; this is used to figure out
	 * whether it exists and what kind of device it is.
	 */
	outb(priv->io_port + ATA_REG_DEVICEHEAD, 0xa0 | (unit << 4));
	TINY_WAIT();
	outb(priv->io_port + ATA_REG_COMMAND, cmd);
	TINY_WAIT();

	/*
	 * Now we wait for BSY to clear, and DRDY to be set. If this times out, we
	 * assume there is no device.
	 */
	int timeout = 50000;

	while (ata_read_status(dev) & ATA_STAT_BSY) {
		TINY_WAIT();
		if (timeout--)
			break;
	}
	while ((ata_read_status(dev) & ATA_STAT_DRDY) == 0) {
		TINY_WAIT();
		if (timeout == 0)
			break;
		timeout--;
	}
	if (timeout == 0) {
		device_printf(dev, "timeout waiting for unit %u", unit);
		return 0;
	}

	/* OK, now we can get the device type */
	int atapi = 0;
	uint8_t cl = inb(priv->io_port + ATA_REG_CYL_LO);
	uint8_t ch = inb(priv->io_port + ATA_REG_CYL_HI);
	if (cl == 0x14 && ch == 0xeb) {
		/* This is a magic identifier for ATAPI devices! */
		atapi++;
	} else if (cl == 0x69 && ch == 0x96) {
		/* This is a magic identifier for SATA-ATAPI devices! */
		atapi++;
	} else if (cl == 0x3c && ch == 0xc3) {
		/* This is a magic identifier for SATA devices! */
	} else if (cl == 0 && ch == 0) {
		/* Plain old ATA disk */
	} else {
		device_printf(dev, "unit %u does not report recognized type (got 0x%x), assuming disk",
			unit, ch << 8 | cl);
	}

	if (ata_read_status(dev) & ATA_STAT_ERR) {
		/* An error is expected in ATAPI-land; they had to introduce a new identify command */
		if (!atapi)
			return 0;
		cmd = ATA_CMD_IDENTIFY_PACKET;

		outb(priv->io_port + ATA_REG_DEVICEHEAD, 0xa0 | (unit << 4));
		TINY_WAIT();
		outb(priv->io_port + ATA_REG_COMMAND, cmd);
		TINY_WAIT();

		/* Wait until ERR is turned on, or !BSY && DRQ */
		int timeout = 5000;
		while (1) {
			uint8_t x = ata_read_status(dev);
			if (!(x & ATA_STAT_BSY) && (x & ATA_STAT_DRQ))
				break;
			if (x & ATA_STAT_ERR)
				return 0;
			timeout--;
		}
		if (timeout == 0) {
			device_printf(dev, "timeout waiting for atapi identification of unit %u", unit);
			return 0;
		}
	}

	/* Grab the result of the identification command XXX implement insw() */
	char* buf = (char*)identify;
	for (int count = 0; count < SECTOR_SIZE; ) {
		uint16_t x = inw(priv->io_port + ATA_REG_DATA);
		buf[count++] = x >> 8;
		buf[count++] = x & 0xff; 
	}

	/* Chop off the trailing spaces off the identity */
	for (int i = sizeof(identify->model) - 1; i > 0; i--) {
		if (identify->model[i] != ' ')
			break;
		identify->model[i] = '\0';
	}

	return cmd;
}

static errorcode_t
ata_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	struct ATA_PRIVDATA* priv = kmalloc(sizeof(struct ATA_PRIVDATA));
	priv->io_port = (uint32_t)(uintptr_t)res_io;
	/* XXX this is a hack - at least, until we properly support multiple resources */
	if (priv->io_port == 0x170) {
		priv->io_port2 = (uint32_t)0x374;
	} else if (priv->io_port == 0x1f0) {
		priv->io_port2 = (uint32_t)0x3f4;
	} else {
		device_printf(dev, "couldn't determine second I/O range");
		return ANANAS_ERROR(NO_RESOURCE);
	}
	QUEUE_INIT(&priv->requests);
	dev->privdata = priv;

	/* Ensure there's something living at the I/O addresses */
	if (inb(priv->io_port + ATA_REG_STATUS) == 0xff) return 1;

	if (!irq_register((uintptr_t)res_irq, dev, ata_irq))
		return ANANAS_ERROR(NO_RESOURCE);

	/* reset the control register - this ensures we receive interrupts */
	outb(priv->io_port + ATA_REG_DEVCONTROL, 0);
	return ANANAS_ERROR_OK;
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

	struct ATA_REQUEST_ITEM* item = QUEUE_HEAD(&priv->requests);
	KASSERT(item->unit >= 0 && item->unit <= 1, "corrupted item number");
	KASSERT(item->count > 0, "corrupted count number");

	if (item->command != ATA_CMD_PACKET) {
		/* Feed the request to the drive - disk */
		outb(priv->io_port + ATA_REG_DEVICEHEAD, 0xe0 | (item->unit ? 0x10 : 0) | ((item->lba >> 24) & 0xf));
		outb(priv->io_port + ATA_REG_SECTORCOUNT, item->count);
		outb(priv->io_port + ATA_REG_SECTORNUM, item->lba & 0xff);
		outb(priv->io_port + ATA_REG_CYL_LO, (item->lba >> 8) & 0xff);
		outb(priv->io_port + ATA_REG_CYL_HI, (item->lba >> 16) & 0xff);
		outb(priv->io_port + ATA_REG_COMMAND, item->command);
	} else {
		/* Feed the request to the device - ATAPI */
		outb(priv->io_port + ATA_REG_DEVICEHEAD, item->unit << 4);
		ATA_DELAY(); ATA_DELAY();
		outb(priv->io_port + ATA_REG_FEATURES, 0); /* no DMA yet! */
		outb(priv->io_port + ATA_REG_CYL_LO, item->count & 0xff); /* note: in bytes! */
		outb(priv->io_port + ATA_REG_CYL_HI, item->count >> 8);
		outb(priv->io_port + ATA_REG_COMMAND, item->command);

		/* Wait until the command is accepted */
		uint8_t status;
		while (ata_read_status(dev) & ATA_STAT_BSY) ;
		while (1) {
			status = ata_read_status(dev);
			if (status & ATA_STAT_ERR) {
				/* Got an error - this means the request cannot be completed */
				bio_set_error(item->bio);
;				return;
			}
			if (status & ATA_STAT_DRQ)
				break;
		}
		for (unsigned int i = 0; i < 6; i++) {
			/* XXX We really need outsw() */
			outw(priv->io_port + ATA_REG_DATA, *(uint16_t*)(&item->atapi_command[i * 2]));
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
		if (driver == NULL)
			continue;

		device_t new_dev = device_alloc(dev, driver);
		new_dev->privdata = (void*)&identify; /* XXX this is a hack; we should have an userpointer */
		device_add_resource(new_dev, RESTYPE_CHILDNUM, unit, 0);
		device_attach_single(new_dev);
	}
}

static void
ata_enqueue(device_t dev, void* request)
{
	struct ATA_PRIVDATA* priv = (struct ATA_PRIVDATA*)dev->privdata;
	struct ATA_REQUEST_ITEM* item = (struct ATA_REQUEST_ITEM*)request;
	KASSERT(item->bio != NULL, "ata_enqueue(): request without bio data buffer");
	/*
	 * XXX Duplicate the request; this should be converted to a preallocated list
	 *     or something someday.
	 */
	struct ATA_REQUEST_ITEM* newitem = kmalloc(sizeof(struct ATA_REQUEST_ITEM));
	memcpy(newitem, request, sizeof(struct ATA_REQUEST_ITEM));
	QUEUE_ADD_TAIL(&priv->requests, newitem);
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
