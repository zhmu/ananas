#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

struct SLICE_PRIVATE {
	device_t biodev;
	block_t	first_block;
	size_t	length;
};

static errorcode_t
slice_read(device_t dev, void* buf, size_t* length, off_t offset)
{
	/*
	 * All we do is grab the request, mangle it and call the parent's read routine.
	 */
	struct SLICE_PRIVATE* privdata = (struct SLICE_PRIVATE*)dev->privdata;
	return dev->parent->driver->drv_read(dev->parent, buf, length, offset + privdata->first_block);
}

struct DRIVER drv_slice = {
	.name	= "slice",
	.drv_read = slice_read
};

struct DEVICE*
slice_create(device_t dev, block_t begin, block_t length)
{
	struct SLICE_PRIVATE* privdata = (struct SLICE_PRIVATE*)kmalloc(sizeof(struct SLICE_PRIVATE));
	privdata->first_block = begin;
	privdata->length = length;

	device_t new_dev = device_alloc(dev, &drv_slice);
	new_dev->privdata = privdata;
	device_attach_single(new_dev);

	return new_dev;
}

/* vim:set ts=2 sw=2: */
