#include <ananas/types.h>
#include "kernel/console-driver.h"
#include "kernel/device.h"
#include "kernel/dev/kbdmux.h"
#include "kernel/dev/tty.h"
#include "kernel/lib.h"

namespace {

struct VConsole : public TTY, private keyboard_mux::IKeyboardConsumer
{
	using TTY::TTY;
	virtual ~VConsole() = default;

	Result Attach() override;
	Result Detach() override;
	Result Print(const char* buffer, size_t len) override;

	void OnCharacter(int ch) override;

	Device* v_VGA = nullptr;
};

Result
VConsole::Attach()
{
	// XXX we assume vga is always available
	v_VGA = Ananas::DeviceManager::AttachChild(*this, d_ResourceSet);
	KASSERT(v_VGA != nullptr, "no vga attached?");

	keyboard_mux::RegisterConsumer(*this);
	return Result::Success();
}

Result
VConsole::Detach()
{
	panic("TODO");
	keyboard_mux::UnregisterConsumer(*this);
	return Result::Success();
}

Result
VConsole::Print(const char* buffer, size_t len)
{
	size_t size = len;
	return v_VGA->GetCharDeviceOperations()->Write(buffer, size, 0);
}

void
VConsole::OnCharacter(int in)
{
	char ch = in;
	OnInput(&ch, 1);
}

struct VConsole_Driver : public Ananas::ConsoleDriver
{
	VConsole_Driver()
	 : ConsoleDriver("vconsole", 100)
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "corebus";
	}

	Ananas::Device* ProbeDevice() override
	{
		return new VConsole(Ananas::ResourceSet());
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return nullptr; // we expect to be probed
	}
};

} // unnamed namespace

REGISTER_DRIVER(VConsole_Driver)

/* vim:set ts=2 sw=2: */
