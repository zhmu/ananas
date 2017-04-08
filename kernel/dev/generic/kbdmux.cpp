#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/tty.h>

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

	errorcode_t Attach() override;
	errorcode_t Detach() override;
	errorcode_t Read(void* buffer, size_t& len, off_t offset) override;

	void OnInput(uint8_t ch);

	/* We must use a spinlock here as kbdmux_on_input() can be called from IRQ context */
	spinlock_t kbd_lock;
	char kbd_buffer[KBDMUX_BUFFER_SIZE];
	int kbd_buffer_readpos = 0;
	int kbd_buffer_writepos = 0;
};

KeyboardMux* kbdmux_instance = NULL; // XXX KLUDGE

void
KeyboardMux::OnInput(uint8_t ch)
{
	/* Add the data to our buffer */
	register_t state = spinlock_lock_unpremptible(&kbd_lock);
	/* XXX we should protect against buffer overruns */
	kbd_buffer[kbd_buffer_writepos] = ch;
	kbd_buffer_writepos = (kbd_buffer_writepos + 1) % KBDMUX_BUFFER_SIZE;
	spinlock_unlock_unpremptible(&kbd_lock, state);

	/* XXX signal consumers - this is a hack */
	if (console_tty != NULL) {
		tty_signal_data();
	}
}

errorcode_t
KeyboardMux::Read(void* data, size_t& len, off_t off)
{
	size_t returned = 0, left = len;

	register_t state = spinlock_lock_unpremptible(&kbd_lock);
	while (left-- > 0) {
		if (kbd_buffer_readpos == kbd_buffer_writepos)
			break;

		*(uint8_t*)(static_cast<uint8_t*>(data) + returned++) = kbd_buffer[kbd_buffer_readpos];
		kbd_buffer_readpos = (kbd_buffer_readpos + 1) % KBDMUX_BUFFER_SIZE;
	}
	spinlock_unlock_unpremptible(&kbd_lock, state);

	len = returned;
	return ananas_success();
}

errorcode_t
KeyboardMux::Attach()
{
	KASSERT(kbdmux_instance == NULL, "multiple kbdmux");
	kbdmux_instance = this;
	spinlock_init(&kbd_lock);

	return ananas_success();
}

errorcode_t
KeyboardMux::Detach()
{
	return ananas_success();
}

struct KeyboardMux_Driver : public Ananas::ConsoleDriver
{
	KeyboardMux_Driver()
	 : ConsoleDriver("kbdmux", 10, CONSOLE_FLAG_IN)
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
