#include <ananas/types.h>
#include <ananas/error.h>
#include <machine/param.h>		/* for PAGE_SIZE */
#include <ananas/bootinfo.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vm.h>

TRACE_SETUP;

struct RAMDISK_PRIVDATA {
	void*		ram_buffer;
	size_t	ram_size;
};

static errorcode_t
ramdisk_probe(device_t dev)
{
	if (bootinfo == NULL || bootinfo->bi_ramdisk_addr == 0)
		return ANANAS_ERROR(NO_DEVICE);
	return ANANAS_ERROR_OK;
}

static errorcode_t
ramdisk_attach(device_t dev)
{
	struct RAMDISK_PRIVDATA* privdata = kmalloc(sizeof(struct RAMDISK_PRIVDATA));
	privdata->ram_buffer = vm_map_kernel(bootinfo->bi_ramdisk_addr, (bootinfo->bi_ramdisk_size + (PAGE_SIZE - 1)) / PAGE_SIZE, VM_FLAG_READ);
	privdata->ram_size = bootinfo->bi_ramdisk_size;
	dev->privdata = privdata;

	kprintf("%s: %u KB\n", dev->name, privdata->ram_size  / 1024);
	return ANANAS_ERROR_OK;
}

static errorcode_t
ramdisk_read(device_t dev, void* buffer, size_t* length, off_t offset)
{
	struct RAMDISK_PRIVDATA* privdata = (struct RAMDISK_PRIVDATA*)dev->privdata;
	KASSERT(*length > 0, "invalid length");
	KASSERT(*length % 512 == 0, "invalid length"); /* XXX */
	KASSERT(buffer != NULL, "invalid buffer");

	KASSERT((offset * 512) + *length < privdata->ram_size, "attempted to read beyond ramdisk range");

	struct BIO* bio = buffer;
	void* addr = (void*)((addr_t)privdata->ram_size + (addr_t)offset * 512);
	memcpy(bio->data, addr, *length);
	bio_set_available(bio);
	return ANANAS_ERROR_OK;
}

struct DRIVER drv_ramdisk = {
	.name					= "ramdisk",
	.drv_probe		= ramdisk_probe,
	.drv_attach		= ramdisk_attach,
	.drv_read			= ramdisk_read
};

DRIVER_PROBE(ramdisk)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
