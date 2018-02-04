#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mbr.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/x86/io.h"
#include "ata.h"
#include "atacd.h"
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

Result
ATACD::Attach()
{
	ata_unit = (int)(uintptr_t)d_ResourceSet.AllocateResource(Ananas::Resource::RT_ChildNum, 0);

	Printf("<%s>", ata_identify.model);

	return Result::Success();
}

Result
ATACD::Detach()
{
	panic("unimplemented");
	return Result::Success();
}

Result
ATACD::ReadBIO(struct BIO& bio)
{
	struct ATA_REQUEST_ITEM item;
	KASSERT(bio.length == 2048, "invalid length"); /* XXX */

	/* XXX boundary check */
	memset(&item, 0, sizeof(item));
	item.unit = ata_unit;
	item.count = bio.length;
	item.bio = &bio;
	item.command = ATA_CMD_PACKET;
	item.flags = ATA_ITEM_FLAG_READ | ATA_ITEM_FLAG_ATAPI;
	item.atapi_command[0] = ATAPI_CMD_READ_SECTORS;
	item.atapi_command[2] = (bio.io_block >> 24) & 0xff;
	item.atapi_command[3] = (bio.io_block >> 16) & 0xff;
	item.atapi_command[4] = (bio.io_block >>  8) & 0xff;
	item.atapi_command[5] = (bio.io_block      ) & 0xff;
	item.atapi_command[7] = (bio.length / 2048) >> 8;
	item.atapi_command[8] = (bio.length / 2048) & 0xff;
	EnqueueAndStart(d_Parent, item);

	return Result::Success();
}

Result
ATACD::WriteBIO(struct BIO& bio)
{
	return RESULT_MAKE_FAILURE(EROFS);
}

struct ATACD_Driver : public Ananas::Driver
{
	ATACD_Driver()
	: Driver("atacd")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr; // instantiated by ata-pci
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new ATACD(cdp);
	}
};

REGISTER_DRIVER(ATACD_Driver)

} // namespace ATA
} // namespace Ananas

/* vim:set ts=2 sw=2: */
