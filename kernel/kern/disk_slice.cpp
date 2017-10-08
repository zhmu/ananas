#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"

namespace {

class Slice : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IBIODeviceOperations
{
public:
	Slice(const Ananas::CreateDeviceProperties& cdp)
	 : Device(cdp)
	{
	}

	void SetFirstBlock(blocknr_t firstBlock)
	{
		slice_first_block = firstBlock;
	}

	void SetLength(blocknr_t length)
	{
		slice_length = length;
	}

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	IBIODeviceOperations* GetBIODeviceOperations() override
	{
		return this;
	}

	errorcode_t Attach() override
	{
		return ananas_success();
	}

	errorcode_t Detach() override
	{
		return ananas_success();
	}

	errorcode_t ReadBIO(struct BIO& bio) override;
	errorcode_t WriteBIO(struct BIO& bio) override;

private:
	blocknr_t	slice_first_block = 0;
	blocknr_t slice_length = 0;
};

errorcode_t
Slice::ReadBIO(struct BIO& bio)
{
	bio.io_block = bio.block + slice_first_block;
	return d_Parent->GetBIODeviceOperations()->ReadBIO(bio);
}

errorcode_t
Slice::WriteBIO(struct BIO& bio)
{
	bio.io_block = bio.block + slice_first_block;
	return d_Parent->GetBIODeviceOperations()->WriteBIO(bio);
}

struct Slice_Driver : public Ananas::Driver
{
	Slice_Driver()
	 : Driver("slice")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr; // instantiated by disk_mbr
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new Slice(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(Slice_Driver)

Ananas::Device*
slice_create(Ananas::Device* parent, blocknr_t begin, blocknr_t length)
{
	auto slice = static_cast<Slice*>(Ananas::DeviceManager::CreateDevice("slice", Ananas::CreateDeviceProperties(*parent, Ananas::ResourceSet())));
	if (slice != nullptr) {
		slice->SetFirstBlock(begin);
		slice->SetLength(length);
		if (ananas_is_failure(Ananas::DeviceManager::AttachSingle(*slice))) {
			delete slice;
			slice = nullptr;
		}
	}
	return slice;
}

/* vim:set ts=2 sw=2: */
