#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bootinfo.h>
#include <ananas/bio.h>
#include <ananas/trace.h>
#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <loader/module.h>
#include <machine/vm.h>

TRACE_SETUP;

namespace {

class RAMDisk : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IBIODeviceOperations
{
public:
	using Device::Device;
	virtual ~RAMDisk() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	IBIODeviceOperations* GetBIODeviceOperations() override
	{
		return this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	errorcode_t ReadBIO(struct BIO& bio) override;
	errorcode_t WriteBIO(struct BIO& bio) override;

private:
	void*		ram_buffer;
	size_t	ram_size;
};

errorcode_t
RAMDisk::Attach()
{
	/* Walk through all modules and attach the first RAM disk we see XXX we should support more */
	for (struct LOADER_MODULE* mod = (struct LOADER_MODULE*)PTOKV((addr_t)bootinfo->bi_modules);
	     (addr_t)mod != PTOKV((addr_t)NULL); mod = (struct LOADER_MODULE*)PTOKV((addr_t)mod->mod_next)) {
		if (mod->mod_type != MOD_RAMDISK)
			continue;

		ram_buffer = (void*)PTOKV((addr_t)mod->mod_phys_start_addr);
		ram_size = (addr_t)mod->mod_phys_end_addr - (addr_t)mod->mod_phys_start_addr;

		Printf("%u KB",
		 (addr_t)mod->mod_phys_start_addr, (addr_t)mod->mod_phys_end_addr,
		 ram_size / 1024);
		
		return ananas_success();
	}
	return ANANAS_ERROR(NO_DEVICE);
}

errorcode_t
RAMDisk::Detach()
{
	panic("Detach");
	return ananas_success();
}

errorcode_t
RAMDisk::ReadBIO(struct BIO& bio)
{
	KASSERT(bio.length > 0, "invalid length");
	KASSERT(bio.length % BIO_SECTOR_SIZE== 0, "invalid length"); /* XXX */

	KASSERT((bio.io_block * BIO_SECTOR_SIZE) + bio.length < ram_size, "attempted to read beyond ramdisk range");

	/* XXX We could really use page-mapped blocks now */
	memcpy(BIO_DATA(&bio), (void*)((addr_t)ram_buffer + (addr_t)bio.io_block * BIO_SECTOR_SIZE), bio.length);

	bio_set_available(&bio);
	return ananas_success();
}

errorcode_t
RAMDisk::WriteBIO(struct BIO& bio)
{
	return ANANAS_ERROR(READ_ONLY);
}

struct RAMDisk_Driver : public Ananas::Driver
{
	RAMDisk_Driver()
	: Driver("ramdisk")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "corebus";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		if (bootinfo == NULL)
			return nullptr;
		return new RAMDisk(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(RAMDisk_Driver)

/* vim:set ts=2 sw=2: */
