#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include "../core/config.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"

namespace {

class USBKeyboard : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::USB::IPipeCallback
{
public:
	using Device::Device;
	virtual ~USBKeyboard() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

protected:
	void OnPipeCallback(Ananas::USB::Pipe& pipe) override;

private:
	Ananas::USB::USBDevice* uk_Device = nullptr;
	Ananas::USB::Pipe* uk_Pipe = nullptr;
};

errorcode_t
USBKeyboard::Attach()
{
	uk_Device = static_cast<Ananas::USB::USBDevice*>(d_ResourceSet.AllocateResource(Ananas::Resource::RT_USB_Device, 0));

	errorcode_t err = uk_Device->AllocatePipe(0, TRANSFER_TYPE_INTERRUPT, EP_DIR_IN, 0, *this, uk_Pipe);
	if (ananas_is_failure(err)) {
		Printf("endpoint 0 not interrupt/in");
		return err;
	}
	return uk_Pipe->Start();
}

errorcode_t
USBKeyboard::Detach()
{
	panic("TODO");
	return ananas_success();
}

void
USBKeyboard::OnPipeCallback(Ananas::USB::Pipe& pipe)
{
	Ananas::USB::Transfer& xfer = pipe.p_xfer;

	Printf("USBKeyboard::OnPipeCallback() -> [");

	if (xfer.t_flags & TRANSFER_FLAG_ERROR) {
		Printf("error, aborting]");
		return;
	}

	for (int i = 0; i < xfer.t_result_length; i++) {
		Printf("%d: %x", i, xfer.t_data[i]);
	}

	/* Reschedule the pipe for future updates */
	uk_Pipe->Start();
}

struct USBKeyboard_Driver : public Ananas::Driver
{
	USBKeyboard_Driver()
	 : Driver("usbkeyboard")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "usbbus";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_USB_Device, 0);
		if (res == nullptr)
			return nullptr;

		auto usb_dev = static_cast<Ananas::USB::USBDevice*>(reinterpret_cast<void*>(res->r_Base));

		Ananas::USB::Interface& iface = usb_dev->ud_interface[usb_dev->ud_cur_interface];
		if (iface.if_class == USB_IF_CLASS_HID && iface.if_protocol == 1 /* keyboard */)
			return new USBKeyboard(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(USBKeyboard_Driver)

/* vim:set ts=2 sw=2: */
