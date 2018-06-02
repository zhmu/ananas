#include "kernel/console-driver.h"
#include "kernel/dev/tty.h"
#include "kernel/lib.h"
#include "vconsole.h"
#include "vga.h"
#include "vtty.h"

Result
VConsole::Attach()
{
	// XXX we assume vga is always available
	v_Video = new VGA;
	KASSERT(v_Video != nullptr, "no video attached?");

	for(auto& vtty: vttys) {
		Ananas::CreateDeviceProperties cdp(*this, Ananas::ResourceSet());
		vtty = static_cast<VTTY*>(Ananas::DeviceManager::CreateDevice("vtty", Ananas::CreateDeviceProperties(*this, Ananas::ResourceSet())));
		if (auto result = Ananas::DeviceManager::AttachSingle(*vtty); result.IsFailure())
			panic("cannot attach vtty (%d)", result.AsStatusCode());
	}
	activeVTTY = vttys.front();
	activeVTTY->Activate();

	keyboard_mux::RegisterConsumer(*this);
	return Result::Success();
}

Result
VConsole::Detach()
{
	panic("TODO");

	for(auto& vtty: vttys) {
		if (auto result = vtty->Detach(); result.IsFailure())
			panic("cannot detach vtty (%d)", result.AsStatusCode());
	}
	keyboard_mux::UnregisterConsumer(*this);
	delete v_Video;
	return Result::Success();
}

Result
VConsole::Read(void* buf, size_t len, off_t offset)
{
	return activeVTTY->Read(buf, len, offset);
}

Result
VConsole::Write(const void* buf, size_t len, off_t offset)
{
	return activeVTTY->Write(buf, len, offset);
}

void
VConsole::OnKey(const keyboard_mux::Key& key, int modifiers)
{
	char ch = key.ch;
	switch(key.type) {
		case keyboard_mux::Key::Type::Character:
			if (modifiers & keyboard_mux::modifier::Control) {
				if (ch >= 'a' && ch <= 'z')
					ch = (ch - 'a') + 1; // control-a => 1, etc
				else
					break; // swallow the key
			}
			activeVTTY->OnInput(&ch, 1);
			break;
		case keyboard_mux::Key::Type::Special:
			if (key.ch >= keyboard_mux::code::F1 && key.ch <= keyboard_mux::code::F12) {
				int desiredVTTY = key.ch - keyboard_mux::code::F1;
				if (desiredVTTY < vttys.size()) {
					activeVTTY->Deactivate();
					activeVTTY = vttys[desiredVTTY];
					activeVTTY->Activate();
				}
			}
			break;
		default:
			break;
	}
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

REGISTER_DRIVER(VConsole_Driver)

/* vim:set ts=2 sw=2: */
