#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/bio.h>
#include <ananas/device.h>
#include <ananas/lib.h>
#include <ananas/mm.h>

namespace {

class Slice : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IBIODeviceOperations
{
public:
	Slice(int unit, Ananas::Device* biodev, blocknr_t first_block, size_t length)
	 : Device(Ananas::CreateDeviceProperties(*biodev, "slice", unit)),
		 slice_biodev(biodev), slice_first_block(first_block), slice_length(length)
	{
		(void)slice_length; // XXX use
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
	Ananas::Device* slice_biodev;
	blocknr_t	slice_first_block;
	size_t slice_length;
};

errorcode_t
Slice::ReadBIO(struct BIO& bio)
{
	bio.io_block = bio.block + slice_first_block;
	return slice_biodev->GetBIODeviceOperations()->ReadBIO(bio);
}

errorcode_t
Slice::WriteBIO(struct BIO& bio)
{
	bio.io_block = bio.block + slice_first_block;
	return slice_biodev->GetBIODeviceOperations()->WriteBIO(bio);
}

} // namespace

Ananas::Device*
slice_create(Ananas::Device* parent, blocknr_t begin, blocknr_t length)
{
	int unit = 0; // XXX
	auto device = new Slice(unit++, parent, begin, length);
	if (ananas_is_failure(Ananas::DeviceManager::AttachSingle(*device))) {
		delete device;
		return NULL;
	}
	return device;
}

/* vim:set ts=2 sw=2: */
