#ifndef __UHCI_HCD_H__
#define __UHCI_HCD_H__

#include <ananas/irq.h>

#define UHCI_FRAMELIST_LEN	(4096 / 4)
#define UHCI_NUM_INTERRUPT_QH	6 /* 1, 2, 4, 8, 16, 32ms queues */

namespace Ananas {
namespace USB {

namespace UHCI {
LIST_DEFINE(HCD_ScheduledItemQueue, struct HCD_ScheduledItem);
LIST_DEFINE(TD_QUEUE, struct HCD_TD);
LIST_DEFINE(QH_QUEUE, struct HCD_QH);

struct HCD_TD;
struct HCD_QH;
} // namespace UHCI

class UHCI_HCD : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::IUSBDeviceOperations
{
public:
	using Device::Device;
	virtual ~UHCI_HCD() = default;

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

	UHCI::HCD_TD* AllocateTD();
	UHCI::HCD_QH* AllocateQH();
	void FreeQH(UHCI::HCD_QH* qh);

	/* Number of root hub ports */
	unsigned int uhci_rh_numports;
	USBDevice* uhci_roothub = nullptr;
	thread_t uhci_rh_pollthread;
	/* I/O port */
	uint32_t uhci_io;
	/* Have we completed a port reset? */
	int uhci_c_portreset = 0;

protected:
	errorcode_t SetupTransfer(Transfer& xfer) override;
	errorcode_t TearDownTransfer(Transfer& xfer) override;
	errorcode_t CancelTransfer(Transfer& xfer) override;
	errorcode_t ScheduleTransfer(Transfer& xfer) override;
	void SetRootHub(USB::USBDevice& dev) override;

	errorcode_t ScheduleControlTransfer(Transfer& xfer);
	errorcode_t ScheduleInterruptTransfer(Transfer& xfer);

	void Dump();
	void OnIRQ();

	static irqresult_t IRQWrapper(Ananas::Device* device, void* context)
	{
		auto uhci_hcd = static_cast<UHCI_HCD*>(device);
		uhci_hcd->OnIRQ();
		return IRQ_RESULT_PROCESSED;
	}

private:
	/* Mutex used to protect the privdata */
	mutex_t uhci_mtx;

	dma_buf_t uhci_framelist_buf;
	uint32_t* uhci_framelist;

	/* Start of frame value */
	uint32_t uhci_sof_modify;
	/* Interrupt/control/bulk QH's */
	struct UHCI::HCD_QH* uhci_qh_interrupt[UHCI_NUM_INTERRUPT_QH];
	struct UHCI::HCD_QH* uhci_qh_ls_control;
	struct UHCI::HCD_QH* uhci_qh_fs_control;
	struct UHCI::HCD_QH* uhci_qh_bulk;
	/* Currently scheduled queue items */
	struct UHCI::HCD_ScheduledItemQueue uhci_scheduled_items;
};

} // namespace USB
} // namespace Ananas

#endif /* __UHCI_HCD_H__ */
