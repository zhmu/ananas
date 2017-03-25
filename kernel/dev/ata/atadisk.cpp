#include <ananas/types.h>
#include <ananas/driver.h>
#include <ananas/error.h>
#include <ananas/x86/io.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/mm.h>
#include <mbr.h>
#include "ata.h"
#include "atadisk.h"
#include "ata-controller.h"

TRACE_SETUP;

namespace Ananas {
namespace ATA {

namespace {

void EnqueueAndStart(Ananas::Device* parent, ATA_REQUEST_ITEM& item)
{
  // XXX this is a hack
  auto atacl = static_cast<Ananas::ATA::ATAController*>(parent);
  atacl->Enqueue(&item);
  atacl->Start();
}

} // unnamed namespace

errorcode_t
ATADisk::Attach()
{
	disk_unit = (int)(uintptr_t)d_ResourceSet.AllocateResource(Ananas::Resource::RT_ChildNum, 0);

	/* Calculate the length of the disk */
	disk_size = ATA_GET_DWORD(disk_identify.lba_sectors);
	if (ATA_GET_WORD(disk_identify.features2) & ATA_FEAT2_LBA48) {
		disk_size  = ATA_GET_QWORD(disk_identify.lba48_sectors);
	}

	Printf("<%s> - %u MB",
	 disk_identify.model,
	 disk_size / ((1024UL * 1024UL) / 512UL));

	/* XXX We should check the registers to see if DMA support is configured */
	if(true) {
		disk_flags |= ATADISK_FLAG_DMA;
		Printf("using DMA transfers");
	}

	/*
	 * Read the first sector and pass it to the MBR code; this is crude
	 * and does not really belong here.
	 */
	struct BIO* bio = bio_read(this, 0, BIO_SECTOR_SIZE);
	if (BIO_IS_ERROR(bio))
		return ANANAS_ERROR(IO); /* XXX should get error from bio */

	mbr_process(this, bio);
	bio_free(bio);
	return ananas_success();
}

errorcode_t
ATADisk::Detach()
{
	panic("unimplemented");
	return ananas_success();
}

errorcode_t
ATADisk::ReadBIO(struct BIO& bio)
{
	struct ATA_REQUEST_ITEM item;
	KASSERT(bio.length > 0, "invalid length");
	KASSERT(bio.length % 512 == 0, "invalid length"); /* XXX */

	/* XXX boundary check */
	item.unit = disk_unit;
	item.lba = bio.io_block;
	item.count = bio.length / 512;
	item.bio = &bio;
	item.command = ATA_CMD_READ_SECTORS;
	item.flags = ATA_ITEM_FLAG_READ;
	if (disk_flags & ATADISK_FLAG_DMA)
		item.flags |= ATA_ITEM_FLAG_DMA;
	EnqueueAndStart(d_Parent, item);

	return ananas_success();
}

errorcode_t
ATADisk::WriteBIO(struct BIO& bio)
{
	struct ATA_REQUEST_ITEM item;
	KASSERT(bio.length > 0, "invalid length");
	KASSERT(bio.length % 512 == 0, "invalid length"); /* XXX */

	/* XXX boundary check */
	item.unit = disk_unit;
	item.lba = bio.io_block;
	item.count = bio.length / 512;
	item.bio = &bio;
	item.command = ATA_CMD_WRITE_SECTORS;
	item.flags = ATA_ITEM_FLAG_WRITE;
#ifdef NOTYET
	if (disk_flags & ATADISK_FLAG_DMA)
		item.flags |= ATA_ITEM_FLAG_DMA;
#endif
	EnqueueAndStart(d_Parent, item);

	return ananas_success();
}

namespace {

struct ATADisk_Driver : public Ananas::Driver
{
	ATADisk_Driver()
	: Driver("atadisk")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr; // instantiated by ata-pci
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new ATADisk(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(ATADisk_Driver)

} // namespace ATA
} // namespace Ananas

/* vim:set ts=2 sw=2: */
