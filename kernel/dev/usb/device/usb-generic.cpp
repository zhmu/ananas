#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include "../core/config.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/pipe.h"

namespace {

class USBGeneric : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	using Device::Device;
	virtual ~USBGeneric() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

private:
	Ananas::USB::USBDevice* ug_Device;
};

errorcode_t
USBGeneric::Attach()
{
	ug_Device = static_cast<Ananas::USB::USBDevice*>(d_ResourceSet.AllocateResource(Ananas::Resource::RT_USB_Device, 0));
	return ananas_success();
}

errorcode_t
USBGeneric::Detach()
{
	panic("TODO");
	return ananas_success();
}

struct USBGeneric_Driver : public Ananas::Driver
{
	USBGeneric_Driver()
	 : Driver("usbgeneric", 1000000)
	{
		// Note that we are a fall-back driver (we'll attach to anything USB), so
		// we have a very high priority value to push us to the end of the driver
		// list
	}

	const char* GetBussesToProbeOn() const override
	{
		return "usbbus";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		// We accept any USB device
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_USB_Device, 0);
		if (res == nullptr)
			return nullptr;

		return new USBGeneric(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(USBGeneric_Driver)

/* vim:set ts=2 sw=2: */
