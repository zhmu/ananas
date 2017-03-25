#ifndef ANANAS_ATA_PCI_H
#define ANANAS_ATA_PCI_H

#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/irq.h>
#include <ananas/dev/ata.h>

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

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	void Enqueue(void* request);
	void Start();

protected:
	void OnIRQ();

	static irqresult_t IRQWrapper(Ananas::Device* device, void* context)
	{
		auto atapci = static_cast<ATAPCI*>(device);
		atapci->OnIRQ();
		return IRQ_RESULT_PROCESSED;
	}

	uint8_t ReadStatus();
	int Identify(int unit, ATA_IDENTIFY& identify);
	void StartPIO(struct ATA_REQUEST_ITEM& item);
	void StartDMA(struct ATA_REQUEST_ITEM& item);

private:
	uint32_t atapci_io;
	uint32_t atapci_dma_io;
	uint32_t atapci_io_alt;

	spinlock_t spl_requests;
	struct ATA_REQUEST_QUEUE requests;
	spinlock_t spl_freelist;
	struct ATA_REQUEST_QUEUE freelist;

	struct ATAPCI_PRDT atapci_prdt[ATA_PCI_NUMPRDT] __attribute__((aligned(4)));
};

} // namespace ATA
} // namespace Ananas


#endif // ANANAS_ATA_PCI_H
