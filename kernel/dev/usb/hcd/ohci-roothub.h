#ifndef __OHCI_ROOTHUB_H__
#define __OHCI_ROOTHUB_H__

namespace Ananas {
namespace USB {

class Transfer;
class USBDevice;

namespace OHCI {

class HCD_Resources;

class RootHub {
public:
	RootHub(HCD_Resources& hcdResources, USBDevice& usbDevice);
	errorcode_t Initialize();

	void SetUSBDevice(USBDevice& usbDevice);
	errorcode_t HandleTransfer(Transfer& xfer);
	void OnIRQ();

protected:
	errorcode_t ControlTransfer(Transfer& xfer);
	void ProcessInterruptTransfers();
	static void ThreadWrapper(void* context)
	{
		(static_cast<RootHub*>(context))->Thread();
	}
	void Thread();

private:
	HCD_Resources& rh_Resources;
	USBDevice& rh_Device;

	int rh_numports;
	Semaphore rh_semaphore;
	struct Thread rh_pollthread;
	bool rh_pending_changes = true;
};

} // namespace OHCI
} // namespace USB
} // namespace Ananas

#endif /* __OHCI_ROOTHUB_H__ */
