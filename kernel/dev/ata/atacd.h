#ifndef ANANAS_ATA_CD_H
#define ANANAS_ATA_CD_H

#include "kernel/device.h"
#include "kernel/dev/ata.h"
#include "ata.h"

namespace ata {

class ATACD : public Device, private IDeviceOperations, private IBIODeviceOperations
{
public:
	using Device::Device;
	virtual ~ATACD() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	IBIODeviceOperations* GetBIODeviceOperations() override
	{
		return this;
	}

	Result Attach() override;
	Result Detach() override;

	Result ReadBIO(struct BIO& bio) override;
	Result WriteBIO(struct BIO& bio) override;

	void SetIdentify(const ATA_IDENTIFY& identify)
	{
		ata_identify = identify;
	}

private:
	int ata_unit;
	ATA_IDENTIFY ata_identify;
};

} // namespace ata

#endif // ANANAS_ATA_CD_H
