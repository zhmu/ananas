#include <ananas/types.h>
#include <ananas/util/array.h>
#include "kernel/dev/kbdmux.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/x86/io.h"
#include "kernel-md/md.h"
#include "options.h"

TRACE_SETUP;

namespace {

using namespace keyboard_mux::code;
namespace modifier = keyboard_mux::modifier;
using Key = keyboard_mux::Key;
using KeyType = keyboard_mux::Key::Type;

namespace scancode {
constexpr uint8_t Escape = 0x01;
constexpr uint8_t Control = 0x1d; // right-control has 0xe0 prefix
constexpr uint8_t Tilde = 0x29;
constexpr uint8_t LeftShift = 0x2a;
constexpr uint8_t RightShift = 0x36;
constexpr uint8_t Alt = 0x38; // right-alt has 0xe0 prefix
constexpr uint8_t Delete = 0x53;
constexpr uint8_t ReleasedBit = 0x80;
}

namespace port {
constexpr int Status = 4;
constexpr int StatusOutputFull = 1;
}

struct KeyMap {
	Key standard;
	Key shift;
};

constexpr util::array<KeyMap, 128> keymap = {
	/* 00 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 01 */ KeyMap{ Key{ KeyType::Character, 0x1b }, Key{ KeyType::Character, 0x1b } },
	/* 02 */ KeyMap{ Key{ KeyType::Character,  '1' }, Key{ KeyType::Character,  '!' } },
	/* 03 */ KeyMap{ Key{ KeyType::Character,  '2' }, Key{ KeyType::Character,  '@' } },
	/* 04 */ KeyMap{ Key{ KeyType::Character,  '3' }, Key{ KeyType::Character,  '#' } },
	/* 05 */ KeyMap{ Key{ KeyType::Character,  '4' }, Key{ KeyType::Character,  '$' } },
	/* 06 */ KeyMap{ Key{ KeyType::Character,  '5' }, Key{ KeyType::Character,  '%' } },
	/* 07 */ KeyMap{ Key{ KeyType::Character,  '6' }, Key{ KeyType::Character,  '^' } },
	/* 08 */ KeyMap{ Key{ KeyType::Character,  '7' }, Key{ KeyType::Character,  '&' } },
	/* 09 */ KeyMap{ Key{ KeyType::Character,  '8' }, Key{ KeyType::Character,  '*' } },
	/* 0a */ KeyMap{ Key{ KeyType::Character,  '9' }, Key{ KeyType::Character,  '(' } },
	/* 0b */ KeyMap{ Key{ KeyType::Character,  '0' }, Key{ KeyType::Character,  ')' } },
	/* 0c */ KeyMap{ Key{ KeyType::Character,  '-' }, Key{ KeyType::Character,  '_' } },
	/* 0d */ KeyMap{ Key{ KeyType::Character,  '=' }, Key{ KeyType::Character,  '+' } },
	/* 0e */ KeyMap{ Key{ KeyType::Character, 0x08 }, Key{ KeyType::Invalid,      0 } },
	/* 0f */ KeyMap{ Key{ KeyType::Character, 0x09 }, Key{ KeyType::Invalid,      0 } },
	/* 10 */ KeyMap{ Key{ KeyType::Character,  'q' }, Key{ KeyType::Character,  'Q' } },
	/* 11 */ KeyMap{ Key{ KeyType::Character,  'w' }, Key{ KeyType::Character,  'W' } },
	/* 12 */ KeyMap{ Key{ KeyType::Character,  'e' }, Key{ KeyType::Character,  'E' } },
	/* 13 */ KeyMap{ Key{ KeyType::Character,  'r' }, Key{ KeyType::Character,  'R' } },
	/* 14 */ KeyMap{ Key{ KeyType::Character,  't' }, Key{ KeyType::Character,  'T' } },
	/* 15 */ KeyMap{ Key{ KeyType::Character,  'y' }, Key{ KeyType::Character,  'Y' } },
	/* 16 */ KeyMap{ Key{ KeyType::Character,  'u' }, Key{ KeyType::Character,  'U' } },
	/* 17 */ KeyMap{ Key{ KeyType::Character,  'i' }, Key{ KeyType::Character,  'I' } },
	/* 18 */ KeyMap{ Key{ KeyType::Character,  'o' }, Key{ KeyType::Character,  'O' } },
	/* 19 */ KeyMap{ Key{ KeyType::Character,  'p' }, Key{ KeyType::Character,  'P' } },
	/* 1a */ KeyMap{ Key{ KeyType::Character,  '[' }, Key{ KeyType::Character,  '{' } },
	/* 1b */ KeyMap{ Key{ KeyType::Character,  ']' }, Key{ KeyType::Character,  '}' } },
	/* 1c */ KeyMap{ Key{ KeyType::Character, 0x0d }, Key{ KeyType::Invalid,      0 } },
	/* 1d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 1e */ KeyMap{ Key{ KeyType::Character,  'a' }, Key{ KeyType::Character,  'A' } },
	/* 1f */ KeyMap{ Key{ KeyType::Character,  's' }, Key{ KeyType::Character,  'S' } },
	/* 20 */ KeyMap{ Key{ KeyType::Character,  'd' }, Key{ KeyType::Character,  'D' } },
	/* 21 */ KeyMap{ Key{ KeyType::Character,  'f' }, Key{ KeyType::Character,  'F' } },
	/* 22 */ KeyMap{ Key{ KeyType::Character,  'g' }, Key{ KeyType::Character,  'G' } },
	/* 23 */ KeyMap{ Key{ KeyType::Character,  'h' }, Key{ KeyType::Character,  'H' } },
	/* 24 */ KeyMap{ Key{ KeyType::Character,  'j' }, Key{ KeyType::Character,  'J' } },
	/* 25 */ KeyMap{ Key{ KeyType::Character,  'k' }, Key{ KeyType::Character,  'K' } },
	/* 26 */ KeyMap{ Key{ KeyType::Character,  'l' }, Key{ KeyType::Character,  'L' } },
	/* 27 */ KeyMap{ Key{ KeyType::Character,  ';' }, Key{ KeyType::Character,  ':' } },
	/* 28 */ KeyMap{ Key{ KeyType::Character, '\'' }, Key{ KeyType::Character,  '"' } },
	/* 29 */ KeyMap{ Key{ KeyType::Character,  '`' }, Key{ KeyType::Character,  '~' } },
	/* 2a */ KeyMap{ Key{ KeyType::Invalid ,     0 }, Key{ KeyType::Invalid,      0 } },
	/* 2b */ KeyMap{ Key{ KeyType::Character, '\\' }, Key{ KeyType::Character,  '|' } },
	/* 2c */ KeyMap{ Key{ KeyType::Character,  'z' }, Key{ KeyType::Character,  'Z' } },
	/* 2d */ KeyMap{ Key{ KeyType::Character,  'x' }, Key{ KeyType::Character,  'X' } },
	/* 2e */ KeyMap{ Key{ KeyType::Character,  'c' }, Key{ KeyType::Character,  'C' } },
	/* 2f */ KeyMap{ Key{ KeyType::Character,  'v' }, Key{ KeyType::Character,  'V' } },
	/* 30 */ KeyMap{ Key{ KeyType::Character,  'b' }, Key{ KeyType::Character,  'B' } },
	/* 31 */ KeyMap{ Key{ KeyType::Character,  'n' }, Key{ KeyType::Character,  'N' } },
	/* 32 */ KeyMap{ Key{ KeyType::Character,  'm' }, Key{ KeyType::Character,  'M' } },
	/* 33 */ KeyMap{ Key{ KeyType::Character,  ',' }, Key{ KeyType::Character,  '<' } },
	/* 34 */ KeyMap{ Key{ KeyType::Character,  '.' }, Key{ KeyType::Character,  '>' } },
	/* 35 */ KeyMap{ Key{ KeyType::Character,  '/' }, Key{ KeyType::Character,  '?' } },
	/* 36 */ KeyMap{ Key{ KeyType::Invalid ,     0 }, Key{ KeyType::Invalid,      0 } },
	/* 37 */ KeyMap{ Key{ KeyType::Character,  '*' }, Key{ KeyType::Character,  '*' } },
	/* 38 */ KeyMap{ Key{ KeyType::Invalid ,     0 }, Key{ KeyType::Invalid,      0 } },
	/* 39 */ KeyMap{ Key{ KeyType::Character,  ' ' }, Key{ KeyType::Character,  ' ' } },
	/* 3a */ KeyMap{ Key{ KeyType::Invalid ,     0 }, Key{ KeyType::Invalid,      0 } },
	/* 3b */ KeyMap{ Key{ KeyType::Special,     F1 }, Key{ KeyType::Special,     F1 } },
	/* 3c */ KeyMap{ Key{ KeyType::Special,     F2 }, Key{ KeyType::Special,     F2 } },
	/* 3d */ KeyMap{ Key{ KeyType::Special,     F3 }, Key{ KeyType::Special,     F3 } },
	/* 3e */ KeyMap{ Key{ KeyType::Special,     F4 }, Key{ KeyType::Special,     F4 } },
	/* 3f */ KeyMap{ Key{ KeyType::Special,     F5 }, Key{ KeyType::Special,     F5 } },
	/* 40 */ KeyMap{ Key{ KeyType::Special,     F6 }, Key{ KeyType::Special,     F6 } },
	/* 41 */ KeyMap{ Key{ KeyType::Special,     F7 }, Key{ KeyType::Special,     F7 } },
	/* 42 */ KeyMap{ Key{ KeyType::Special,     F8 }, Key{ KeyType::Special,     F8 } },
	/* 43 */ KeyMap{ Key{ KeyType::Special,     F9 }, Key{ KeyType::Special,     F9 } },
	/* 44 */ KeyMap{ Key{ KeyType::Special,    F10 }, Key{ KeyType::Special,    F10 } },
	/* 45 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 46 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 47 */ KeyMap{ Key{ KeyType::Character,  '7' }, Key{ KeyType::Character,  '7' } },
	/* 48 */ KeyMap{ Key{ KeyType::Character,  '8' }, Key{ KeyType::Character,  '8' } },
	/* 49 */ KeyMap{ Key{ KeyType::Character,  '9' }, Key{ KeyType::Character,  '9' } },
	/* 4a */ KeyMap{ Key{ KeyType::Character,  '-' }, Key{ KeyType::Character,  '-' } },
	/* 4b */ KeyMap{ Key{ KeyType::Character,  '4' }, Key{ KeyType::Character,  '4' } },
	/* 4c */ KeyMap{ Key{ KeyType::Character,  '5' }, Key{ KeyType::Character,  '5' } },
	/* 4d */ KeyMap{ Key{ KeyType::Character,  '6' }, Key{ KeyType::Character,  '6' } },
	/* 4e */ KeyMap{ Key{ KeyType::Character,  '+' }, Key{ KeyType::Character,  '+' } },
	/* 4f */ KeyMap{ Key{ KeyType::Character,  '1' }, Key{ KeyType::Character,  '1' } },
	/* 50 */ KeyMap{ Key{ KeyType::Character,  '2' }, Key{ KeyType::Character,  '2' } },
	/* 51 */ KeyMap{ Key{ KeyType::Character,  '3' }, Key{ KeyType::Character,  '3' } },
	/* 52 */ KeyMap{ Key{ KeyType::Character,  '0' }, Key{ KeyType::Character,  '0' } },
	/* 53 */ KeyMap{ Key{ KeyType::Character,  '.' }, Key{ KeyType::Character,  '.' } },
	/* 54 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 55 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 56 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Invalid,      0 } },
	/* 57 */ KeyMap{ Key{ KeyType::Special,    F11 }, Key{ KeyType::Special,    F11 } },
	/* 58 */ KeyMap{ Key{ KeyType::Special,    F12 }, Key{ KeyType::Special,    F12 } },
	/* 59 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,  ' ' } },
	/* 5a */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,  ' ' } },
	/* 5b */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 5c */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 5d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 5e */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 5f */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 60 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 61 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 62 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 63 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 64 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 65 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 66 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 67 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 68 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 69 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 6a */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 6b */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 6c */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 6d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 6e */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 6f */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 70 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 71 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 72 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 73 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 74 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 75 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 76 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 77 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 78 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 79 */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 7a */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 7b */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 7c */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 7d */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 7e */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
	/* 7f */ KeyMap{ Key{ KeyType::Invalid,      0 }, Key{ KeyType::Character,    0 } },
};

class ATKeyboard : public Device, private IDeviceOperations, private irq::IHandler
{
public:
	using Device::Device;
	virtual ~ATKeyboard() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override;
	Result Detach() override;

private:
	irq::IRQResult OnIRQ() override;

private:
	int kbd_ioport;
	int	kbd_modifiers = 0;
};

irq::IRQResult
ATKeyboard::OnIRQ()
{
	while(inb(kbd_ioport + port::Status) & port::StatusOutputFull) {
		uint8_t scancode = inb(kbd_ioport);
		bool isReleased = (scancode & scancode::ReleasedBit) != 0;
		scancode &= ~scancode::ReleasedBit;

		// Handle setting control, alt, shift flags
		{
			auto setOrClearModifier = [&](int flag) {
				if (isReleased)
					kbd_modifiers &= ~flag;
				else
					kbd_modifiers |= flag;
			};

			switch(scancode) {
				case scancode::LeftShift:
				case scancode::RightShift:
					setOrClearModifier(modifier::Shift);
					continue;
				case scancode::Alt:
					setOrClearModifier(modifier::Alt);
					continue;
				case scancode::Control:
					setOrClearModifier(modifier::Control);
					continue;
			}
		}
		if (isReleased)
			continue; // ignore released keys

#ifdef OPTION_KDB
		if ((kbd_modifiers == (modifier::Control | modifier::Shift)) && scancode == scancode::Escape) {
			kdb::Enter("keyboard sequence");
			continue;
		}
		if (kbd_modifiers == modifier::Control && scancode == scancode::Tilde)
			panic("forced by keyboard");
#endif

		if (kbd_modifiers == (modifier::Control | modifier::Alt) && scancode == scancode::Delete)
			md::Reboot();

		// Look up the scancode
		const auto& key = [](int scancode, int modifiers) {
			const auto& km = keymap[scancode];
			if (modifiers & modifier::Control)
				return km.standard;

			if (modifiers & modifier::Shift)
				return km.shift;

			return km.standard;
		}(scancode, kbd_modifiers);

		if (key.IsValid())
			keyboard_mux::OnKey(key, kbd_modifiers);
	}
	return irq::IRQResult::Processed;
}

Result
ATKeyboard::Attach()
{
	void* res_io = d_ResourceSet.AllocateResource(Resource::RT_IO, 7);
	if (res_io == nullptr)
		return RESULT_MAKE_FAILURE(ENODEV);

	// Initialize private data; must be done before the interrupt is registered
	kbd_ioport = (uintptr_t)res_io;
	if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
		return result;

	/*
	 * Ensure the keyboard's input buffer is empty; this will cause it to
	 * send IRQ's to us.
	 */
	inb(kbd_ioport);
	return Result::Success();
}

Result
ATKeyboard::Detach()
{
	panic("detach");
	return Result::Success();
}

struct ATKeyboard_Driver : public Driver
{
	ATKeyboard_Driver()
	 : Driver("atkbd")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Device* CreateDevice(const CreateDeviceProperties& cdp) override
	{
	auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PNP_ID, 0);
	if (res != NULL && res->r_Base == 0x0303) /* PNP0303: IBM Enhanced (101/102-key, PS/2 mouse support) */
		return new ATKeyboard(cdp);
	return nullptr;
	}
};

const RegisterDriver<ATKeyboard_Driver> registerDriver;

} // unnamed namespace

/* vim:set ts=2 sw=2: */
