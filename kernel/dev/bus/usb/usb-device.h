#ifndef __ANANAS_USB_DEVICE_H__
#define __ANANAS_USB_DEVICE_H__

#include "usb-core.h"

namespace Ananas {

class Device;

namespace USB {

class Bus;
class Hub;
class Transfer;

/*
 * An USB device consists of a name, an address, pipes and a driver. We
 * augment the existing device for this and consider the USB_DEVICE as the
 * private data for the device in question.
 *
 * Fields marked with [S] are static and won't change after initialization; fields
 * with [M] use the mutex to protect them.
 */
class USBDevice
{
public:
	USBDevice(Bus& bus, Hub* hub, int hub_port, int flags);
	~USBDevice();

	USBDevice(const USBDevice&) = delete;
	USBDevice& operator=(const USBDevice&) = delete;

	static const size_t s_DefaultMaxPacketSz0 = 8;

	Bus&	ud_bus;			/* [S] Bus the device resides on */
	Hub*	ud_hub;			/* [S] Hub the device is attached to, or nullptr for the root hub */
	int	ud_port;			/* [S] Port of the hub the device is attached to */
	Device*	ud_device = nullptr;		/* [S] Device reference */
	int	ud_flags;			/* [S] Device flags */
	int	ud_address = 0;		   	/* Assigned USB address */
	int	ud_max_packet_sz0 = s_DefaultMaxPacketSz0;		/* Maximum packet size for endpoint 0 */
	Interface	ud_interface[USB_MAX_INTERFACES];
	int	ud_num_interfaces = 0;
	int	ud_cur_interface = -1;
	struct USB_DESCR_DEVICE ud_descr_device;
	struct Pipes ud_pipes;			/* [M] */

//	void*	ud_hcd_privdata = nullptr;

	errorcode_t Attach();
	errorcode_t Detach(); // called with bus lock held

	/* Pending transfers for this device */
	TransferQueue ud_transfers;		/* [M] */

	/* Provide link fields for device list */
	LIST_FIELDS(USBDevice);

	void Lock()
	{
		mutex_lock(&ud_mutex);
	}

	void Unlock()
	{
		mutex_unlock(&ud_mutex);
	}

	void AssertLocked()
	{
		mutex_assert(&ud_mutex, MTX_LOCKED);
	}

private:
	mutex_t ud_mutex;
};

#define USB_DEVICE_FLAG_LOW_SPEED (1 << 0)
#define USB_DEVICE_FLAG_ROOT_HUB (1 << 31)

} // namespace USB
} // namespace Ananas

#endif /* __ANANAS_USB_DEVICE_H__ */
