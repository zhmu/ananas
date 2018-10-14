#ifndef ANANAS_ATA_CONTROLLER_H
#define ANANAS_ATA_CONTROLLER_H

#include "kernel/dev/ata.h"
#include "kernel/irq.h"
#include "kernel/device.h"
#include "ata.h"

namespace ata {

class ATAController : public Device, private IDeviceOperations
{
public:
	using Device::Device;
	virtual ~ATAController() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override;
	Result Detach() override;

	void Enqueue(void* request);
	void Start();

protected:
	void OnIRQ();

	static irqresult_t IRQWrapper(Device* device, void* context)
	{
		auto ata = static_cast<ATAController*>(device);
		ata->OnIRQ();
		return IRQ_RESULT_PROCESSED;
	}

	uint8_t ReadStatus();
	int Identify(int unit, ATA_IDENTIFY& identify);
	void StartPIO(struct ATA_REQUEST_ITEM& item);
	void StartDMA(struct ATA_REQUEST_ITEM& item);

private:
	uint32_t ata_io;
	uint32_t ata_dma_io;
	uint32_t ata_io_alt;

	spinlock_t spl_requests;
	struct ATA_REQUEST_QUEUE requests;
	spinlock_t spl_freelist;
	struct ATA_REQUEST_QUEUE freelist;

	struct ATAPCI_PRDT ata_prdt[ATA_PCI_NUMPRDT] __attribute__((aligned(4)));
};

} // namespace ata

#endif // ANANAS_ATA_CONTROLLER_H
