#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bootinfo.h>
#include <ananas/bio.h>
#include <ananas/trace.h>
#include <ananas/device.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <loader/module.h>
#include <machine/vm.h>

TRACE_SETUP;

struct RAMDISK_PRIVDATA {
	void*		ram_buffer;
	size_t	ram_size;
};

static errorcode_t
ramdisk_probe(device_t dev)
{
	if (bootinfo == NULL)
		return ANANAS_ERROR(NO_DEVICE);
	return ananas_success();
}

static errorcode_t
ramdisk_attach(device_t dev)
{
	/* Walk through all modules and attach the first RAM disk we see XXX we should support more */
	for (struct LOADER_MODULE* mod = (struct LOADER_MODULE*)PTOKV((addr_t)bootinfo->bi_modules);
	     (addr_t)mod != PTOKV((addr_t)NULL); mod = (struct LOADER_MODULE*)PTOKV((addr_t)mod->mod_next)) {
		if (mod->mod_type != MOD_RAMDISK)
			continue;

		auto privdata = static_cast<struct RAMDISK_PRIVDATA*>(kmalloc(sizeof(struct RAMDISK_PRIVDATA)));

		privdata->ram_buffer = (void*)PTOKV((addr_t)mod->mod_phys_start_addr);
		privdata->ram_size = (addr_t)mod->mod_phys_end_addr - (addr_t)mod->mod_phys_start_addr;
		dev->privdata = privdata;

		device_printf(dev, "%u KB",
		 (addr_t)mod->mod_phys_start_addr, (addr_t)mod->mod_phys_end_addr,
		 privdata->ram_size / 1024);
		
		return ananas_success();
	}
	return ANANAS_ERROR(NO_DEVICE);
}

static errorcode_t
ramdisk_bread(device_t dev, struct BIO* bio)
{
	struct RAMDISK_PRIVDATA* privdata = (struct RAMDISK_PRIVDATA*)dev->privdata;
	KASSERT(bio->length > 0, "invalid length");
	KASSERT(bio->length % BIO_SECTOR_SIZE== 0, "invalid length"); /* XXX */

	KASSERT((bio->io_block * BIO_SECTOR_SIZE) + bio->length < privdata->ram_size, "attempted to read beyond ramdisk range");

	/* XXX We could really use page-mapped blocks now */
	memcpy(BIO_DATA(bio), (void*)((addr_t)privdata->ram_buffer + (addr_t)bio->io_block * BIO_SECTOR_SIZE), bio->length);

	bio_set_available(bio);
	return ananas_success();
}

struct DRIVER drv_ramdisk = {
	.name					= "ramdisk",
	.drv_probe		= ramdisk_probe,
	.drv_attach		= ramdisk_attach,
	.drv_bread		= ramdisk_bread
};

DRIVER_PROBE(ramdisk)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
