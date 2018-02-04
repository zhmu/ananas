#ifndef ANANAS_ATA_PCI_H
#define ANANAS_ATA_PCI_H

#include "kernel/device.h"
#include "ata.h"

namespace Ananas {
namespace ATA {

class ATAPCI : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	using Device::Device;
	virtual ~ATAPCI() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override;
	Result Detach() override;
};

} // namespace ATA
} // namespace Ananas

#endif // ANANAS_ATA_PCI_H
