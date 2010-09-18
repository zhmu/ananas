#include <machine/io.h>
#include <ananas/dev/ata.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <mbr.h>

struct ATACD_PRIVDATA {
	int unit;
};

static int
atacd_attach(device_t dev)
{
	int unit = (int)device_alloc_resource(dev, RESTYPE_CHILDNUM, 0);
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)dev->privdata;

	struct ATACD_PRIVDATA* priv = kmalloc(sizeof(struct ATACD_PRIVDATA));
	dev->privdata = priv;
	priv->unit = unit;

	device_printf(dev, "<%s>", identify->model);

	return 1;
}

static ssize_t
atacd_read(device_t dev, void* buffer, size_t length, off_t offset)
{
	struct ATACD_PRIVDATA* priv = (struct ATACD_PRIVDATA*)dev->privdata;
	struct ATA_REQUEST_ITEM item;
	KASSERT(length == 2048, "invalid length"); /* XXX */

	/* XXX boundary check */
	memset(&item, 0, sizeof(item));
	item.unit = priv->unit;
	item.count = length;
	item.bio = (struct BIO*)buffer;
	item.command = ATA_CMD_PACKET;
	item.atapi_command[0] = ATAPI_CMD_READ_SECTORS;
	item.atapi_command[2] = (offset >> 24) & 0xff;
	item.atapi_command[3] = (offset >> 16) & 0xff;
	item.atapi_command[4] = (offset >>  8) & 0xff;
	item.atapi_command[5] = (offset      ) & 0xff;
	item.atapi_command[7] = (length / 2048) >> 8;
	item.atapi_command[8] = (length / 2048) & 0xff;
	dev->parent->driver->drv_enqueue(dev->parent, &item);
	dev->parent->driver->drv_start(dev->parent);
}

struct DRIVER drv_atacd = {
	.name					= "atacd",
	.drv_probe		= NULL,
	.drv_attach		= atacd_attach,
	.drv_read			= atacd_read
};

/* vim:set ts=2 sw=2: */
