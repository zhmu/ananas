#ifndef ANANAS_ATA_CD_H
#define ANANAS_ATA_CD_H

#include <ananas/dev/ata.h>
#include "ata.h"

namespace Ananas {
namespace ATA {

class ATACD : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IBIODeviceOperations
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

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	errorcode_t ReadBIO(struct BIO& bio) override;
	errorcode_t WriteBIO(struct BIO& bio) override;

	void SetIdentify(const ATA_IDENTIFY& identify)
	{
		ata_identify = identify;
	}

private:
	int ata_unit;
	ATA_IDENTIFY ata_identify;
};

} // namespace ATA
} // namespace Ananas

#endif // ANANAS_ATA_CD_H
