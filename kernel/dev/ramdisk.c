#include <ananas/types.h>
#include <ananas/error.h>
#include <machine/param.h>		/* for PAGE_SIZE */
#include <ananas/bootinfo.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/vm.h>

static int
ramdisk_probe(device_t dev)
{
	if (bootinfo == NULL || bootinfo->bi_ramdisk_addr == 0)
		return 1;
	return 0;
}

static int
ramdisk_attach(device_t dev)
{
	kprintf("%s: %u KB\n", dev->name, bootinfo->bi_ramdisk_size / 1024);
	uint32_t len = (bootinfo->bi_ramdisk_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
	vm_map(bootinfo->bi_ramdisk_addr, len);
	return 0;
}

static int
ramdisk_read(device_t dev, void* buffer, size_t* length, off_t offset)
{
	KASSERT(*length > 0, "invalid length");
	KASSERT(*length % 512 == 0, "invalid length"); /* XXX */
	KASSERT(buffer != NULL, "invalid buffer");

	KASSERT((offset * 512) + *length < bootinfo->bi_ramdisk_size, "attempted to read beyond ramdisk range");

	struct BIO* bio = buffer;
	void* addr = (void*)((addr_t)bootinfo->bi_ramdisk_addr + (addr_t)offset * 512);
	memcpy(bio->data, addr, *length);
	bio->flags &= ~BIO_FLAG_DIRTY;
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
