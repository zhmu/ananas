#include "kernel/dev/kbdmux.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/reboot.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/tty.h"
#include "kernel/x86/io.h"
#include "options.h"

TRACE_SETUP;

namespace {

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

namespace flag {
constexpr int Shift = 1;
constexpr int Control = 2;
constexpr int Alt = 4;
}

namespace port {
constexpr int Status = 4;
constexpr int StatusOutputFull = 1;
}

// Mapping of a scancode to an ASCII key value
constexpr uint8_t atkbd_keymap[128] = {
	/* 00-07 */    0, 0x1b, '1',  '2',  '3', '4', '5', '6',
	/* 08-0f */  '7',  '8', '9',  '0',  '-', '=',   8,   9,
	/* 10-17 */  'q',  'w', 'e',  'r',  't', 'y', 'u', 'i',
	/* 18-1f */  'o',  'p', '[',  ']', 0x0d,   0, 'a', 's',
	/* 20-27 */  'd',  'f', 'g',  'h',  'j', 'k', 'l', ';',
	/* 28-2f */  '\'',  '`',   0, '\\', 'z', 'x', 'c', 'v',
	/* 30-37 */  'b',  'n', 'm',  ',',  '.', '/',   0, '*',
	/* 38-3f */    0,  ' ',   0,    0,    0,   0,   0,   0,
	/* 40-47 */    0,    0,   0,    0,    0,   0,   0, '7',
	/* 48-4f */  '8',  '9', '-',  '4',  '5', '6', '+', '1',
	/* 50-57 */  '2',  '3', '0',  '.',    0,   0, 'Z',   0,
	/* 57-5f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 60-67 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 68-6f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 70-76 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 77-7f */    0,    0,   0,    0,    0,   0,   0,   0
};

// Mapping of a scancode to an ASCII key value, if shift is active
constexpr uint8_t atkbd_keymap_shift[128] = {
	/* 00-07 */    0, 0x1b, '!',  '@',  '#', '$', '%', '^',
	/* 08-0f */  '&',  '*', '(',  ')',  '_', '+',   8,   9,
	/* 10-17 */  'Q',  'W', 'E',  'R',  'T', 'Y', 'U', 'I',
	/* 18-1f */  'O',  'P', '{',  '}', 0x0d,   0, 'A', 'S',
	/* 20-27 */  'D',  'F', 'G',  'H',  'J', 'K', 'L', ':',
	/* 28-2f */  '"',  '~',   0,  '|',  'Z', 'X', 'C', 'V',
	/* 30-37 */  'B',  'N', 'M',  '<',  '>', '?',   0, '*',
	/* 38-3f */    0,  ' ',   0,    0,    0,   0,   0,   0,
	/* 40-47 */    0,    0,   0,    0,    0,   0,   0, '7',
	/* 48-4f */  '8',  '9', '-',  '4',  '5', '6', '+', '1',
	/* 50-57 */  '2',  '3', '0',  '.',    0,   0, 'Z',   0,
	/* 57-5f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 60-67 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 68-6f */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 70-76 */    0,    0,   0,    0,    0,   0,   0,   0,
	/* 77-7f */    0,    0,   0,    0,    0,   0,   0,   0
};

class ATKeyboard : public Ananas::Device, private Ananas::IDeviceOperations
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
	void OnIRQ();

	static IRQResult IRQWrapper(Ananas::Device* device, void* context)
	{
		auto atkbd = static_cast<ATKeyboard*>(device);
		atkbd->OnIRQ();
		return IRQResult::IR_Processed;
	}

private:
	int kbd_ioport;
	uint8_t	kbd_flags;
};

void
ATKeyboard::OnIRQ()
{
	while(inb(kbd_ioport + port::Status) & port::StatusOutputFull) {
		uint8_t scancode = inb(kbd_ioport);
		bool isReleased = (scancode & scancode::ReleasedBit) != 0;
		scancode &= ~scancode::ReleasedBit;

		// Handle setting control, alt, shift flags
		{
			auto setOrClearFlag = [&](int flag) {
				if (isReleased)
					kbd_flags &= ~flag;
				else
					kbd_flags |= flag;
			};

			switch(scancode) {
				case scancode::LeftShift:
				case scancode::RightShift:
					setOrClearFlag(flag::Shift);
					continue;
				case scancode::Alt:
					setOrClearFlag(flag::Alt);
					continue;
				case scancode::Control:
					setOrClearFlag(flag::Control);
					continue;
			}
		}
		if (isReleased)
			continue; // ignore released keys

#ifdef OPTION_KDB
		if ((kbd_flags == (flag::Control | flag::Shift)) && scancode == scancode::Escape) {
			kdb_enter("keyboard sequence");
			continue;
		}
		if (kbd_flags == flag::Control && scancode == scancode::Tilde)
			panic("forced by kdb");
#endif

		if (kbd_flags == (flag::Control | flag::Alt) && scancode == scancode::Delete)
			md_reboot();

		// Look up the scancode - if this yields anything, store the key
		uint8_t ch = ((kbd_flags & flag::Shift) ? atkbd_keymap_shift : atkbd_keymap)[scancode];
		if (ch != 0)
			kbdmux_on_input(ch);
	}
}

Result
ATKeyboard::Attach()
{
	void* res_io = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IO, 7);
	void* res_irq = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return RESULT_MAKE_FAILURE(ENODEV);

	// Initialize private data; must be done before the interrupt is registered
	kbd_ioport = (uintptr_t)res_io;
	kbd_flags = 0;

	RESULT_PROPAGATE_FAILURE(
		irq_register((uintptr_t)res_irq, this, IRQWrapper, IRQ_TYPE_DEFAULT, NULL)
	);

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

struct ATKeyboard_Driver : public Ananas::Driver
{
	ATKeyboard_Driver()
	 : Driver("atkbd")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
	auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_PNP_ID, 0);
	if (res != NULL && res->r_Base == 0x0303) /* PNP0303: IBM Enhanced (101/102-key, PS/2 mouse support) */
		return new ATKeyboard(cdp);
	return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(ATKeyboard_Driver)

/* vim:set ts=2 sw=2: */
