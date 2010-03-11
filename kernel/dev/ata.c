#include <machine/io.h>
#include <sys/dev/ata.h>
#include <sys/device.h>
#include <sys/irq.h>
#include <sys/lib.h>

#define ATA_GET_WORD(x) ((uint16_t)(x)[0] << 8 | (x)[1])

int ata_port = 0;

void
ata_irq(device_t dev)
{
	kprintf("ata_irq: TODO\n");
}

#if 0
static int
ata_read_sectors(device_t dev, int slave, uint32_t lba, uint32_t count)
{
	outb(ata_port + ATA_PIO_DRIVEHEAD, 0xa0 | (slave ? 0x10 : 0) | ((lba >> 24) & 0xf));
	/*outb(ata_port + ATA_PIO_ERROR, 0);*/
	outb(ata_port + ATA_PIO_COUNT, count);
	outb(ata_port + ATA_PIO_LBA_1, lba & 0xff);
	outb(ata_port + ATA_PIO_LBA_2, (lba >> 8) & 0xff);
	outb(ata_port + ATA_PIO_LBA_3, (lba >> 16) & 0xff);
	outb(ata_port + ATA_PIO_STATCMD, ATA_CMD_READ_SECTORS);

	while (count--) {
		/* XXX polling */
		while (1) {
			int x = inb(ata_port + ATA_PIO_STATCMD);
			if (!(x & ATA_PIO_STATUS_BSY) && (x & ATA_PIO_STATUS_DRQ))
				break;
			if (x & ATA_PIO_STATUS_ERR)
				return 0;
		}
		char buf[512];
		int count = 0 ;
		while (count < 512) {
			int x = inw(ata_port + ATA_PIO_DATA);
			buf[count++] = x >> 8; buf[count++] = x & 0xff; 
		}
		for (count = 0; count < 512; count++) {
			kprintf("%x ", buf[count]);
		}
		kprintf("\n");
	}

	return 1;
}
#endif

static int
ata_identify(device_t dev, int slave)
{
	outb(ata_port + ATA_PIO_DRIVEHEAD, 0xa0 | (slave ? 0x10 : 0));
	outb(ata_port + ATA_PIO_STATCMD, ATA_CMD_IDENTIFY);
	int x = inb(ata_port + ATA_PIO_STATCMD);
	if (x == 0)
		return 0;

	/* Wait until ERR is turned on, or !BSY && DRQ */
	while (1) {
		x = inb(ata_port + ATA_PIO_STATCMD);
		if (!(x & ATA_PIO_STATUS_BSY) && (x & ATA_PIO_STATUS_DRQ))
			break;
		if (x & ATA_PIO_STATUS_ERR)
			return 0;
	}

	/* Grab the result of the identification command */
	char buf[sizeof(struct ATA_IDENTIFY)];
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)buf;
	int count = 0 ;
	while (count < 512) {
		x = inw(ata_port + ATA_PIO_DATA);
		buf[count++] = x >> 8; buf[count++] = x & 0xff; 
	}

	/* Calculate the length of the disk */
	unsigned long size = ATA_GET_WORD(identify->lba_sectors);
	if (ATA_GET_WORD(identify->features2) & ATA_FEAT2_LBA48) {
		size  = ATA_GET_WORD(identify->lba48_sectors);
		size |= (uint32_t)ATA_GET_WORD(identify->lba48_sectors) >> 16;
	}

	kprintf("model [%s] - foo\n", identify->model);
	kprintf("size: %u sectors (%u MB)\n", size, size / ((1024 * 1024) / 512));
	/* Add the device to our bus */
/*
	device_t new_dev = device_alloc(dev, NULL);
	device_add_resource(new_bus, RESTYPE_PCI_BUS, bus, 0);
*/

//	ata_read_sectors(dev, 0, 1, 1);
	return 1;
}

static int
ata_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_IO, 7);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return 1; /* XXX */
	ata_port = (uintptr_t)res_io;

	/* Ensure there's something living at the I/O addresses */
	if (inb(ata_port + ATA_PIO_STATCMD) == 0xff) return 1;

	if (!irq_register((uintptr_t)res_irq, dev, ata_irq))
		return 1;

	ata_identify(dev, 0);
	return 0;
}

static ssize_t
ata_read(device_t dev, char* data, size_t len)
{
	return 0;
}

static ssize_t
ata_write(device_t dev, const char* data, size_t len)
{
	return 0;
}

struct DRIVER drv_ata = {
	.name					= "ata",
	.drv_probe		= NULL,
	.drv_attach		= ata_attach,
	.drv_read			= ata_read,
	.drv_write		= ata_write
};

DRIVER_PROBE(ata)
DRIVER_PROBE_BUS(isa)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
