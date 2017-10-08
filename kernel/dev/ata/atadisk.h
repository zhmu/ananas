#ifndef ANANAS_ATA_DISK_H
#define ANANAS_ATA_DISK_H

#include "kernel/dev/ata.h"
#include "kernel/device.h"

namespace Ananas {
namespace ATA {

class ATADisk : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IBIODeviceOperations
{
public:
	using Device::Device;
	virtual ~ATADisk() = default;

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

	void SetIdentify(const ATA_IDENTIFY& identify)
	{
		disk_identify = identify;
	}

private:
	ATA_IDENTIFY disk_identify;
	int disk_unit;
	uint64_t disk_size;	/* in sectors */
	uint32_t disk_flags = 0;
#define ATADISK_FLAG_DMA	(1 << 0)
};

} // namespace ATA
} // namespace Ananas

#endif // ANANAS_ATA_DISK_H
