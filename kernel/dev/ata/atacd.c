#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/dev/ata.h>
#include <ananas/x86/io.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <mbr.h>

struct ATACD_PRIVDATA {
	int unit;
};

static errorcode_t
atacd_attach(device_t dev)
{
	int unit = (int)device_alloc_resource(dev, RESTYPE_CHILDNUM, 0);
	struct ATA_IDENTIFY* identify = (struct ATA_IDENTIFY*)dev->privdata;

	struct ATACD_PRIVDATA* priv = kmalloc(sizeof(struct ATACD_PRIVDATA));
	dev->privdata = priv;
	priv->unit = unit;

	device_printf(dev, "<%s>", identify->model);

	return ANANAS_ERROR_OK;
}

static errorcode_t
atacd_bread(device_t dev, struct BIO* bio)
{
	struct ATACD_PRIVDATA* priv = (struct ATACD_PRIVDATA*)dev->privdata;
	struct ATA_REQUEST_ITEM item;
	KASSERT(bio->length == 2048, "invalid length"); /* XXX */

	/* XXX boundary check */
	memset(&item, 0, sizeof(item));
	item.unit = priv->unit;
	item.count = bio->length;
	item.bio = bio;
	item.command = ATA_CMD_PACKET;
	item.atapi_command[0] = ATAPI_CMD_READ_SECTORS;
	item.atapi_command[2] = (bio->io_block >> 24) & 0xff;
	item.atapi_command[3] = (bio->io_block >> 16) & 0xff;
	item.atapi_command[4] = (bio->io_block >>  8) & 0xff;
	item.atapi_command[5] = (bio->io_block      ) & 0xff;
	item.atapi_command[7] = (bio->length / 2048) >> 8;
	item.atapi_command[8] = (bio->length / 2048) & 0xff;
	dev->parent->driver->drv_enqueue(dev->parent, &item);
	dev->parent->driver->drv_start(dev->parent);
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_atacd = {
	.name					= "atacd",
	.drv_probe		= NULL,
	.drv_attach		= atacd_attach,
	.drv_bread		= atacd_bread
};

/* vim:set ts=2 sw=2: */
