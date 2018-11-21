#include <ananas/types.h>
#include <ananas/util/array.h>
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

using namespace keyboard_mux::code;
namespace modifier = keyboard_mux::modifier;
using Key = keyboard_mux::Key;
using KeyType = keyboard_mux::Key::Type;

// Modifier byte is defined in HID 1.11 8.3
namespace modifierByte {
constexpr uint8_t LeftControl = (1 << 0);
constexpr uint8_t LeftShift = (1 << 1);
constexpr uint8_t LeftAlt = (1 << 2);
constexpr uint8_t LeftGui = (1 << 3);
constexpr uint8_t RightControl = (1 << 4);
constexpr uint8_t RightShift = (1 << 5);
constexpr uint8_t RightAlt = (1 << 6);
constexpr uint8_t RightGui = (1 << 7);
}

struct KeyMap {
	Key standard;
	Key shift;
};

// As outlined in USB HID Usage Tables 1.12, chapter 10
constexpr util::array<KeyMap, 128> keymap{
	/* 00 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 01 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 02 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 03 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 04 */ KeyMap{ Key{ KeyType::Character,  'a' }, Key{ KeyType::Character,  'A' } },
	/* 05 */ KeyMap{ Key{ KeyType::Character,  'b' }, Key{ KeyType::Character,  'B' } },
	/* 06 */ KeyMap{ Key{ KeyType::Character,  'c' }, Key{ KeyType::Character,  'C' } },
	/* 07 */ KeyMap{ Key{ KeyType::Character,  'd' }, Key{ KeyType::Character,  'D' } },
	/* 08 */ KeyMap{ Key{ KeyType::Character,  'e' }, Key{ KeyType::Character,  'E' } },
	/* 09 */ KeyMap{ Key{ KeyType::Character,  'f' }, Key{ KeyType::Character,  'F' } },
	/* 0a */ KeyMap{ Key{ KeyType::Character,  'g' }, Key{ KeyType::Character,  'G' } },
	/* 0b */ KeyMap{ Key{ KeyType::Character,  'h' }, Key{ KeyType::Character,  'H' } },
	/* 0c */ KeyMap{ Key{ KeyType::Character,  'i' }, Key{ KeyType::Character,  'I' } },
	/* 0d */ KeyMap{ Key{ KeyType::Character,  'j' }, Key{ KeyType::Character,  'J' } },
	/* 0e */ KeyMap{ Key{ KeyType::Character,  'k' }, Key{ KeyType::Character,  'K' } },
	/* 0f */ KeyMap{ Key{ KeyType::Character,  'l' }, Key{ KeyType::Character,  'L' } },
	/* 10 */ KeyMap{ Key{ KeyType::Character,  'm' }, Key{ KeyType::Character,  'M' } },
	/* 11 */ KeyMap{ Key{ KeyType::Character,  'n' }, Key{ KeyType::Character,  'N' } },
	/* 12 */ KeyMap{ Key{ KeyType::Character,  'o' }, Key{ KeyType::Character,  'O' } },
	/* 13 */ KeyMap{ Key{ KeyType::Character,  'p' }, Key{ KeyType::Character,  'P' } },
	/* 14 */ KeyMap{ Key{ KeyType::Character,  'q' }, Key{ KeyType::Character,  'Q' } },
	/* 15 */ KeyMap{ Key{ KeyType::Character,  'r' }, Key{ KeyType::Character,  'R' } },
	/* 16 */ KeyMap{ Key{ KeyType::Character,  's' }, Key{ KeyType::Character,  'S' } },
	/* 17 */ KeyMap{ Key{ KeyType::Character,  't' }, Key{ KeyType::Character,  'T' } },
	/* 18 */ KeyMap{ Key{ KeyType::Character,  'u' }, Key{ KeyType::Character,  'U' } },
	/* 19 */ KeyMap{ Key{ KeyType::Character,  'v' }, Key{ KeyType::Character,  'V' } },
	/* 1a */ KeyMap{ Key{ KeyType::Character,  'w' }, Key{ KeyType::Character,  'W' } },
	/* 1b */ KeyMap{ Key{ KeyType::Character,  'x' }, Key{ KeyType::Character,  'X' } },
	/* 1c */ KeyMap{ Key{ KeyType::Character,  'y' }, Key{ KeyType::Character,  'Y' } },
	/* 1d */ KeyMap{ Key{ KeyType::Character,  'z' }, Key{ KeyType::Character,  'Z' } },
	/* 1e */ KeyMap{ Key{ KeyType::Character,  '1' }, Key{ KeyType::Character,  '!' } },
	/* 1f */ KeyMap{ Key{ KeyType::Character,  '2' }, Key{ KeyType::Character,  '@' } },
	/* 20 */ KeyMap{ Key{ KeyType::Character,  '3' }, Key{ KeyType::Character,  '#' } },
	/* 21 */ KeyMap{ Key{ KeyType::Character,  '4' }, Key{ KeyType::Character,  '$' } },
	/* 22 */ KeyMap{ Key{ KeyType::Character,  '5' }, Key{ KeyType::Character,  '%' } },
	/* 23 */ KeyMap{ Key{ KeyType::Character,  '6' }, Key{ KeyType::Character,  '^' } },
	/* 24 */ KeyMap{ Key{ KeyType::Character,  '7' }, Key{ KeyType::Character,  '&' } },
	/* 25 */ KeyMap{ Key{ KeyType::Character,  '8' }, Key{ KeyType::Character,  '*' } },
	/* 26 */ KeyMap{ Key{ KeyType::Character,  '9' }, Key{ KeyType::Character,  '(' } },
	/* 27 */ KeyMap{ Key{ KeyType::Character,  '0' }, Key{ KeyType::Character,  ')' } },
	/* 28 */ KeyMap{ Key{ KeyType::Character, 0x0d }, Key{ KeyType::Character, 0x0d } },
	/* 29 */ KeyMap{ Key{ KeyType::Character, 0x1b }, Key{ KeyType::Character, 0x1b } },
	/* 2a */ KeyMap{ Key{ KeyType::Character, 0x08 }, Key{ KeyType::Character, 0x08 } },
	/* 2b */ KeyMap{ Key{ KeyType::Character, 0x09 }, Key{ KeyType::Character, 0x09 } },
	/* 2c */ KeyMap{ Key{ KeyType::Character,  ' ' }, Key{ KeyType::Character,  ' ' } },
	/* 2d */ KeyMap{ Key{ KeyType::Character,  '-' }, Key{ KeyType::Character,  '_' } },
	/* 2e */ KeyMap{ Key{ KeyType::Character,  '=' }, Key{ KeyType::Character,  '+' } },
	/* 2f */ KeyMap{ Key{ KeyType::Character,  '[' }, Key{ KeyType::Character,  '{' } },
	/* 30 */ KeyMap{ Key{ KeyType::Character,  ']' }, Key{ KeyType::Character,  '}' } },
	/* 31 */ KeyMap{ Key{ KeyType::Character, '\\' }, Key{ KeyType::Character,  '|' } },
	/* 32 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 33 */ KeyMap{ Key{ KeyType::Character,  ';' }, Key{ KeyType::Character,  ':' } },
	/* 34 */ KeyMap{ Key{ KeyType::Character, '\'' }, Key{ KeyType::Character,  '"' } },
	/* 35 */ KeyMap{ Key{ KeyType::Character,  '`' }, Key{ KeyType::Character,  '~' } },
	/* 36 */ KeyMap{ Key{ KeyType::Character,  ',' }, Key{ KeyType::Character,  '<' } },
	/* 37 */ KeyMap{ Key{ KeyType::Character,  '.' }, Key{ KeyType::Character,  '>' } },
	/* 38 */ KeyMap{ Key{ KeyType::Character,  '/' }, Key{ KeyType::Character,  '?' } },
	/* 39 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 3a */ KeyMap{ Key{ KeyType::Special,     F1 }, Key{ KeyType::Special,     F1 } },
	/* 3b */ KeyMap{ Key{ KeyType::Special,     F2 }, Key{ KeyType::Special,     F2 } },
	/* 3c */ KeyMap{ Key{ KeyType::Special,     F3 }, Key{ KeyType::Special,     F3 } },
	/* 3d */ KeyMap{ Key{ KeyType::Special,     F4 }, Key{ KeyType::Special,     F4 } },
	/* 3e */ KeyMap{ Key{ KeyType::Special,     F5 }, Key{ KeyType::Special,     F5 } },
	/* 3f */ KeyMap{ Key{ KeyType::Special,     F6 }, Key{ KeyType::Special,     F6 } },
	/* 40 */ KeyMap{ Key{ KeyType::Special,     F7 }, Key{ KeyType::Special,     F7 } },
	/* 41 */ KeyMap{ Key{ KeyType::Special,     F8 }, Key{ KeyType::Special,     F8 } },
	/* 42 */ KeyMap{ Key{ KeyType::Special,     F9 }, Key{ KeyType::Special,     F9 } },
	/* 43 */ KeyMap{ Key{ KeyType::Special,    F10 }, Key{ KeyType::Special,    F10 } },
	/* 44 */ KeyMap{ Key{ KeyType::Special,    F11 }, Key{ KeyType::Special,    F11 } },
	/* 45 */ KeyMap{ Key{ KeyType::Special,    F12 }, Key{ KeyType::Special,    F12 } },
	/* 46 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 47 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 48 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 49 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 4a */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 4b */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 4c */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 4d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 4e */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 4f */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 50 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 51 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 52 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 53 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 54 */ KeyMap{ Key{ KeyType::Character,  '/' }, Key{ KeyType::Character,  '/' } },
	/* 55 */ KeyMap{ Key{ KeyType::Character,  '*' }, Key{ KeyType::Character,  '*' } },
	/* 56 */ KeyMap{ Key{ KeyType::Character,  '-' }, Key{ KeyType::Character,  '-' } },
	/* 57 */ KeyMap{ Key{ KeyType::Character,  '+' }, Key{ KeyType::Character,  '+' } },
	/* 58 */ KeyMap{ Key{ KeyType::Character, 0x0d }, Key{ KeyType::Character, 0x0d }  },
	/* 59 */ KeyMap{ Key{ KeyType::Character,  '1' }, Key{ KeyType::Character,  '1' } },
	/* 5a */ KeyMap{ Key{ KeyType::Character,  '2' }, Key{ KeyType::Character,  '2' } },
	/* 5b */ KeyMap{ Key{ KeyType::Character,  '3' }, Key{ KeyType::Character,  '3' } },
	/* 5c */ KeyMap{ Key{ KeyType::Character,  '4' }, Key{ KeyType::Character,  '4' } },
	/* 5d */ KeyMap{ Key{ KeyType::Character,  '5' }, Key{ KeyType::Character,  '5' } },
	/* 5e */ KeyMap{ Key{ KeyType::Character,  '6' }, Key{ KeyType::Character,  '6' } },
	/* 5f */ KeyMap{ Key{ KeyType::Character,  '7' }, Key{ KeyType::Character,  '7' } },
	/* 60 */ KeyMap{ Key{ KeyType::Character,  '8' }, Key{ KeyType::Character,  '8' } },
	/* 61 */ KeyMap{ Key{ KeyType::Character,  '9' }, Key{ KeyType::Character,  '9' } },
	/* 62 */ KeyMap{ Key{ KeyType::Character,  '0' }, Key{ KeyType::Character,  '0' } },
	/* 63 */ KeyMap{ Key{ KeyType::Character,  '.' }, Key{ KeyType::Character,  '.' } },
	/* 64 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 65 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 66 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 67 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 68 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 69 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 6a */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 6b */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 6c */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 6d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 6e */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 6f */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 70 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 71 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 72 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 73 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 74 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 75 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 76 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 77 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 78 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 79 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 7a */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 7b */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 7c */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 7d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 7e */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 7f */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
};

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
		int scancode = xfer.t_data[n];
		if (scancode == 0 || scancode > keymap.size())
			continue;

		const int modifiers = [](int d) {
			int m = 0;
			if (d & (modifierByte::LeftShift | modifierByte::RightShift))
				m |= modifier::Shift;
			if (d & (modifierByte::LeftControl | modifierByte::RightControl))
				m |= modifier::Control;
			if (d & (modifierByte::LeftAlt | modifierByte::RightAlt))
				m |= modifier::Alt;
			return m;
		}(xfer.t_data[0]);

		// Look up the scancode
		const auto& key = [](int scancode, int modifiers) {
			const auto& km = keymap[scancode];
			if (modifiers & modifier::Control)
				return km.standard;

			if (modifiers & modifier::Shift)
				return km.shift;

			return km.standard;
		}(scancode, modifiers);

		if (key.IsValid())
			keyboard_mux::OnKey(key, modifiers);
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

const RegisterDriver<USBKeyboard_Driver> registerDriver;

} // unnamed namespace

/* vim:set ts=2 sw=2: */
