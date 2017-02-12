#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

struct SLICE_PRIVATE {
	device_t biodev;
	blocknr_t	first_block;
	size_t	length;
};

static errorcode_t
slice_bread(device_t dev, struct BIO* bio)
{
	/*
	 * All we do is grab the request, mangle it and call the parent's read routine.
	 */
	struct SLICE_PRIVATE* privdata = (struct SLICE_PRIVATE*)dev->privdata;
	bio->io_block = bio->block + privdata->first_block;
	return device_bread(dev->parent, bio);
}

static errorcode_t
slice_bwrite(device_t dev, struct BIO* bio)
{
	/*
	 * All we do is grab the request, mangle it and call the parent's read routine.
	 */
	struct SLICE_PRIVATE* privdata = (struct SLICE_PRIVATE*)dev->privdata;
	bio->io_block = bio->block + privdata->first_block;
	return device_bwrite(dev->parent, bio);
}

static struct DRIVER drv_slice = {
	.name	= "slice",
	.drv_bread = slice_bread,
	.drv_bwrite = slice_bwrite
};

struct DEVICE*
slice_create(device_t dev, blocknr_t begin, blocknr_t length)
{
	device_t new_dev = device_alloc(dev, &drv_slice);
	auto privdata = new(new_dev) SLICE_PRIVATE;
	privdata->first_block = begin;
	privdata->length = length;

	new_dev->privdata = privdata;
	device_attach_single(new_dev);

	return new_dev;
}

/* vim:set ts=2 sw=2: */
