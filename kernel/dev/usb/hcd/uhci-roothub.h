#ifndef __UHCI_HUB_H__
#define __UHCI_HUB_H__

namespace Ananas {
namespace USB {

class Transfer;
class USBDevice;
class HCD_Resources;

namespace UHCI {

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
	void UpdateStatus();

private:
	HCD_Resources& rh_Resources;
	USBDevice& rh_Device;

	unsigned int rh_numports = 0;
	bool rh_c_portreset = false;
	struct Thread rh_pollthread;
};

} // namespace UHCI
} // namespace USB
} // namespace Ananas

#endif /* __UHCI_HUB_H__ */
