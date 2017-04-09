#ifndef __ANANAS_USB_BUS_H__
#define __ANANAS_USB_BUS_H__

#include <ananas/list.h>

namespace Ananas {

class Device;

namespace USB {

class USBDevice;
class Hub;
LIST_DEFINE(USBDevices, USBDevice);

/*
 * Single USB bus - directly connected to a HCD.
 */
class Bus : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	using Device::Device;
	virtual ~Bus() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	bool bus_NeedsExplore = false;

	/* List of all USB devices on this bus */
	USBDevices bus_devices;

	int AllocateAddress();

	void ScheduleExplore();
	void Explore();
	errorcode_t DetachHub(Hub& hub);

	/* Link fields for attach or bus list */
	LIST_FIELDS(Bus);

	void Lock()
	{
		mutex_lock(&bus_mutex);
	}

	void Unlock()
	{
		mutex_unlock(&bus_mutex);
	}

	void AssertLocked()
	{
		mutex_assert(&bus_mutex, MTX_LOCKED);
	}

private:
	/* Mutex protecting the bus */
	mutex_t bus_mutex;
};

/* Initialize the usbbus kernel thread which handles device exploring */
void InitializeBus();
void ScheduleAttach(USBDevice& usb_dev);

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_USB_BUS_H__ */
