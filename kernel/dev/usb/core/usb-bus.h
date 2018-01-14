#ifndef __ANANAS_USB_BUS_H__
#define __ANANAS_USB_BUS_H__

#include <ananas/util/list.h>
#include "kernel/device.h"

namespace Ananas {

class Device;

namespace USB {

class USBDevice;
class Hub;
typedef util::List<USBDevice> USBDeviceList;

/*
 * Single USB bus - directly connected to a HCD.
 */
class Bus : public Ananas::Device, private Ananas::IDeviceOperations, public util::List<Bus>::NodePtr
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
	USBDeviceList bus_devices;

	int AllocateAddress();

	void ScheduleExplore();
	void Explore();
	errorcode_t DetachHub(Hub& hub);

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

void ScheduleAttach(USBDevice& usb_dev);

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_USB_BUS_H__ */
