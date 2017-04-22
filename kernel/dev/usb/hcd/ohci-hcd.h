#ifndef __ANANAS_OHCI_HCD_H__
#define __ANANAS_OHCI_HCD_H__

#include <ananas/types.h>
#include <ananas/irq.h>
#include <ananas/thread.h>
#include <ananas/dma.h>

#define OHCI_NUM_ED_LISTS 6 /* 1, 2, 4, 8, 16 and 32ms list */

namespace Ananas {
namespace USB {

class Transfer;
class USBDevice;

namespace OHCI {

struct HCD_TD {
	struct OHCI_TD td_td;
	dma_buf_t td_buf;
	uint32_t td_length;
	LIST_FIELDS(struct HCD_TD);
};

struct HCD_ED {
	struct OHCI_ED ed_ed;
	/* Virtual addresses of the TD chain */
	struct HCD_TD* ed_headtd;
	struct HCD_TD* ed_tailtd;
	/* Virtual addresses of the ED chain */
	struct HCD_ED* ed_preved;
	struct HCD_ED* ed_nexted;
	dma_buf_t ed_buf;
	Transfer* ed_xfer;
	/* Active queue fields; used by ohci_irq() to check all active transfers */
	LIST_FIELDS_IT(struct HCD_ED, active);
};

LIST_DEFINE(HCD_ED_QUEUE, struct HCD_ED);

class HCD_Resources
{
public:
	HCD_Resources()
	 : ohci_membase(nullptr)
	{
	}

	HCD_Resources(uint8_t* membase)
	 : ohci_membase(membase)
	{
	}

	inline void Write2(unsigned int reg, uint16_t value)
	{
		*(volatile uint16_t*)(ohci_membase + reg) = value;
	}

	inline void Write4(unsigned int reg, uint32_t value)
	{
		*(volatile uint32_t*)(ohci_membase + reg) = value;
	}

	inline uint16_t Read2(unsigned int reg)
	{
		return *(volatile uint16_t*)(ohci_membase + reg);
	}

	inline uint32_t Read4(unsigned int reg)
	{
		return *(volatile uint32_t*)(ohci_membase + reg);
	}

private:
	volatile uint8_t* ohci_membase;
};

class RootHub;

} // namespace OHCI

class OHCI_HCD : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IUSBDeviceOperations
{
public:
	using Device::Device;
	virtual ~OHCI_HCD() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	IUSBDeviceOperations* GetUSBDeviceOperations() override
	{
		return this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;
	void DebugDump() override
	{
		Dump();
	}

	errorcode_t SetupTransfer(Transfer& xfer) override;
	errorcode_t TearDownTransfer(Transfer& xfer) override;
	errorcode_t ScheduleTransfer(Transfer& xfer) override;
	errorcode_t CancelTransfer(Transfer& xfer) override;
	void SetRootHub(USB::USBDevice& dev) override;

protected:
	void Dump();
	errorcode_t Setup();

	void CreateTDs(Transfer& xfer);

private:
	void Lock()
	{
		mutex_lock(&ohci_mtx);
	}

	void Unlock()
	{
		mutex_unlock(&ohci_mtx);
	}

	OHCI::HCD_TD* AllocateTD();
	OHCI::HCD_ED* AllocateED();
	OHCI::HCD_ED* SetupED(Transfer& xfer);

	void OnIRQ();

	static irqresult_t IRQWrapper(Ananas::Device* device, void* context)
	{
		auto ohci = static_cast<OHCI_HCD*>(device);
		ohci->OnIRQ();
		return IRQ_RESULT_PROCESSED;
	}

	dma_buf_t ohci_hcca_buf;
	struct OHCI_HCCA* ohci_hcca = nullptr;
	struct OHCI::HCD_ED* ohci_interrupt_ed[OHCI_NUM_ED_LISTS];
	struct OHCI::HCD_ED* ohci_control_ed = nullptr;
	struct OHCI::HCD_ED* ohci_bulk_ed = nullptr;
	struct OHCI::HCD_ED_QUEUE ohci_active_eds;
	mutex_t ohci_mtx;

	OHCI::HCD_Resources ohci_Resources;
	OHCI::RootHub* ohci_RootHub = nullptr;
};

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_OHCI_HCD_H__ */
