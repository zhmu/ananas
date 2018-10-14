#include <ananas/types.h>
#include "kernel/dev/kbdmux.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "../core/config.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"

namespace {

// As outlined in USB HID Usage Tables 1.12, chapter 10
uint8_t usb_keymap[128] = {
	/* 00-07 */    0,    0,   0,    0,  'a', 'b', 'c', 'd',
	/* 08-0f */  'e',  'f', 'g',  'h',  'i', 'j', 'k', 'l',
	/* 10-17 */  'm',  'n', 'o',  'p',  'q', 'r', 's', 't',
	/* 18-1f */  'u',  'v', 'w',  'x',  'y', 'z', '1', '2',
	/* 20-27 */  '3',  '4', '5',  '6',  '7', '8', '9', '0',
	/* 28-2f */   13,  27,    8,    9,  ' ', '-', '=', '[',
	/* 30-37 */  ']', '\\',   0,  ';', '\'', '`', ',', '.',
	/* 38-3f */  '/',    0,   0,    0,    0,   0,   0,   0,
	/* 40-47 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 48-4f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 50-57 */    0,    0,   0,    0,  '/', '*', '-', '+',
	/* 58-5f */   13,  '1', '2',  '3',  '4', '5', '6', '7',
	/* 60-67 */  '8',  '9', '0',  '.',    0,   0,   0,   0,
	/* 68-6f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 70-76 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 77-7f */    0,    0,   0,    0,    0,   0,   0,   0
};

uint8_t usb_keymap_shift[128] = {
	/* 00-07 */    0,    0,   0,    0,  'A', 'B', 'C', 'D',
	/* 08-0f */  'E',  'F', 'G',  'H',  'I', 'J', 'K', 'L',
	/* 10-17 */  'M',  'N', 'O',  'P',  'Q', 'R', 'S', 'T',
	/* 18-1f */  'U',  'V', 'W',  'X',  'Y', 'Z', '!', '@',
	/* 20-27 */  '#',  '$', '%',  '^',  '&', '*', '(', ')',
	/* 28-2f */   13,  27,    8,    9,  ' ', '_', '+', '{',
	/* 30-37 */  '}',  '|',   0,  ':',  '"', '~', '<', '>',
	/* 38-3f */  '?',    0,   0,    0,    0,   0,   0,   0,
	/* 40-47 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 48-4f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 50-57 */    0,    0,   0,    0,  '/', '*',  '-','+',
	/* 58-5f */   13,  '1', '2',  '3',  '4', '5', '6', '7',
	/* 60-67 */  '8',  '9', '0',  '.',    0,   0,   0,   0,
	/* 68-6f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 70-76 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 77-7f */    0,    0,   0,    0,    0,   0,   0,   0
};

// Modifier byte is defined in HID 1.11 8.3
#define MODIFIER_LEFT_CONTROL (1 << 0)
#define MODIFIER_LEFT_SHIFT (1 << 1)
#define MODIFIER_LEFT_ALT (1 << 2)
#define MODIFIER_LEFT_GUI (1 << 3)
#define MODIFIER_RIGHT_CONTROL (1 << 4)
#define MODIFIER_RIGHT_SHIFT (1 << 5)
#define MODIFIER_RIGHT_ALT (1 << 6)
#define MODIFIER_RIGHT_GUI (1 << 7)

class USBKeyboard : public Device, private IDeviceOperations, private usb::IPipeCallback
{
public:
	using Device::Device;
	virtual ~USBKeyboard() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override;
	Result Detach() override;

protected:
	void OnPipeCallback(usb::Pipe& pipe) override;

private:
	usb::USBDevice* uk_Device = nullptr;
	usb::Pipe* uk_Pipe = nullptr;
};

Result
USBKeyboard::Attach()
{
	uk_Device = static_cast<usb::USBDevice*>(d_ResourceSet.AllocateResource(Resource::RT_USB_Device, 0));

	if (auto result = uk_Device->AllocatePipe(0, TRANSFER_TYPE_INTERRUPT, EP_DIR_IN, 0, *this, uk_Pipe); result.IsFailure()) {
		Printf("endpoint 0 not interrupt/in");
		return result;
	}
	return uk_Pipe->Start();
}

Result
USBKeyboard::Detach()
{
	if (uk_Device == nullptr)
		return Result::Success();

	if (uk_Pipe != nullptr)
		uk_Device->FreePipe(*uk_Pipe);

	uk_Pipe = nullptr;
	return Result::Success();
}

void
USBKeyboard::OnPipeCallback(usb::Pipe& pipe)
{
	usb::Transfer& xfer = pipe.p_xfer;

	if (xfer.t_flags & TRANSFER_FLAG_ERROR)
		return;

	// See if there's anything worthwhile to report here. We lazily use the USB boot class as it's much
	// easier to process: HID 1.1 B.1 Protocol 1 (keyboard) lists everything
	for (int n = 2; n < 8; n++) {
		int key = xfer.t_data[n];
		if (key == 0 || key > 128)
			continue;

		uint8_t modifier = xfer.t_data[0];
		bool is_shift = (modifier & (MODIFIER_LEFT_SHIFT | MODIFIER_RIGHT_SHIFT)) != 0;
		const uint8_t* map = is_shift ? usb_keymap_shift : usb_keymap;
		uint8_t ch = map[key];
		if (ch != 0)
			keyboard_mux::OnKey(keyboard_mux::Key(keyboard_mux::Key::Type::Character, ch), 0);
	}

	/* Reschedule the pipe for future updates */
	uk_Pipe->Start();
}

struct USBKeyboard_Driver : public Driver
{
	USBKeyboard_Driver()
	 : Driver("usbkeyboard")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "usbbus";
	}

	Device* CreateDevice(const CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_USB_Device, 0);
		if (res == nullptr)
			return nullptr;

		auto usb_dev = static_cast<usb::USBDevice*>(reinterpret_cast<void*>(res->r_Base));

		auto& iface = usb_dev->ud_interface[usb_dev->ud_cur_interface];
		if (iface.if_class == USB_IF_CLASS_HID && iface.if_subclass == 1 /* boot interface */ && iface.if_protocol == 1 /* keyboard */)
			return new USBKeyboard(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(USBKeyboard_Driver)

/* vim:set ts=2 sw=2: */
