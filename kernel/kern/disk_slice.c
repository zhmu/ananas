#include <sys/types.h>
#include <sys/bio.h>
#include <sys/device.h>
#include <sys/lib.h>
#include <sys/mm.h>

struct SLICE_PRIVATE {
	device_t biodev;
	block_t	first_block;
	size_t	length;
};

static void
slice_start(device_t dev)
{
	/*
	 * All we do is grab the request, mangle it and call the parent's start routine.
	 */
	struct SLICE_PRIVATE* privdata = (struct SLICE_PRIVATE*)dev->privdata;
	struct BIO* bio = bio_get_next(dev);
	KASSERT(bio != NULL, "slice_start without pending requests");

	/* Update the request */
	bio->block += privdata->first_block;
	bio->device = privdata->biodev;

	/* chain the request to our I/O driver */
	privdata->biodev->driver->drv_start(bio->device);
}

struct DRIVER drv_slice = {
	.name	= "slice",
	.drv_start = slice_start 
};

struct DEVICE*
slice_create(device_t dev, block_t begin, block_t length)
{
	struct SLICE_PRIVATE* privdata = (struct SLICE_PRIVATE*)kmalloc(sizeof(struct SLICE_PRIVATE));
	privdata->first_block = begin;
	privdata->length = length;

	device_t new_dev = device_alloc(dev, &drv_slice);
	privdata->biodev = dev->biodev;	/* force I/O to go through our parent */
	new_dev->privdata = privdata;
	device_attach_single(new_dev);
	device_print_attachment(new_dev);

	return new_dev;
}

/* vim:set ts=2 sw=2: */
