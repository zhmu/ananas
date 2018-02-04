#include "kernel/console.h"
#include "kernel/console-driver.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/tty.h"

namespace {

#define KBDMUX_BUFFER_SIZE 16

class KeyboardMux : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations
{
public:
	using Device::Device;
	virtual ~KeyboardMux() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

  ICharDeviceOperations* GetCharDeviceOperations() override
	{
		return this;
	}

	Result Attach() override;
	Result Detach() override;
	Result Read(void* buffer, size_t& len, off_t offset) override;

	void OnInput(uint8_t ch);

private:
	/* We must use a spinlock here as kbdmux_on_input() can be called from IRQ context */
	Spinlock kbd_lock;
	char kbd_buffer[KBDMUX_BUFFER_SIZE];
	int kbd_buffer_readpos = 0;
	int kbd_buffer_writepos = 0;
};

KeyboardMux* kbdmux_instance = NULL; // XXX KLUDGE

void
KeyboardMux::OnInput(uint8_t ch)
{
	// Add the data to our buffer
	{
		SpinlockUnpremptibleGuard g(kbd_lock);
		/* XXX we should protect against buffer overruns */
		kbd_buffer[kbd_buffer_writepos] = ch;
		kbd_buffer_writepos = (kbd_buffer_writepos + 1) % KBDMUX_BUFFER_SIZE;
	}

	/* XXX signal consumers - this is a hack */
	if (console_tty != NULL) {
		tty_signal_data();
	}
}

Result
KeyboardMux::Read(void* data, size_t& len, off_t off)
{
	size_t returned = 0, left = len;

	{
		SpinlockUnpremptibleGuard g(kbd_lock);
		while (left-- > 0) {
			if (kbd_buffer_readpos == kbd_buffer_writepos)
				break;

			*(uint8_t*)(static_cast<uint8_t*>(data) + returned++) = kbd_buffer[kbd_buffer_readpos];
			kbd_buffer_readpos = (kbd_buffer_readpos + 1) % KBDMUX_BUFFER_SIZE;
		}
	}

	len = returned;
	return Result::Success();
}

Result
KeyboardMux::Attach()
{
	KASSERT(kbdmux_instance == NULL, "multiple kbdmux");
	kbdmux_instance = this;

	return Result::Success();
}

Result
KeyboardMux::Detach()
{
	return Result::Success();
}

struct KeyboardMux_Driver : public Ananas::ConsoleDriver
{
	KeyboardMux_Driver()
	 : ConsoleDriver("kbdmux", 100, CONSOLE_FLAG_IN)
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "corebus";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		if (kbdmux_instance != nullptr)
			return nullptr; // allow only a single keyboard mux
		return new KeyboardMux(cdp);
	}

	Ananas::Device* ProbeDevice() override
	{
		return new KeyboardMux(Ananas::CreateDeviceProperties(Ananas::ResourceSet()));
	}
};

} // unnamed namespace

void
kbdmux_on_input(uint8_t ch)
{
	KASSERT(kbdmux_instance != NULL, "no instance?");
	kbdmux_instance->OnInput(ch);
}

REGISTER_DRIVER(KeyboardMux_Driver)

/* vim:set ts=2 sw=2: */
