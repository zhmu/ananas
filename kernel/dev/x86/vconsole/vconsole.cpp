#include <ananas/types.h>
#include <ananas/util/array.h>
#include "kernel/console-driver.h"
#include "kernel/device.h"
#include "kernel/dev/kbdmux.h"
#include "kernel/dev/tty.h"
#include "kernel/lib.h"
#include "vga.h"
#include "vtty.h"

namespace {

struct VConsole : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations, private keyboard_mux::IKeyboardConsumer
{
	using Device::Device;
	virtual ~VConsole() = default;

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

	Result Read(void* buf, size_t& len, off_t offset) override;
	Result Write(const void* buf, size_t& len, off_t offset) override;

	void OnKey(const keyboard_mux::Key& key) override;

private:
	constexpr static size_t NumberOfVTTYs = 1;
	util::array<VTTY*, NumberOfVTTYs> vttys;

	IVideo* v_Video = nullptr;
	VTTY* activeVTTY = nullptr;
};

Result
VConsole::Attach()
{
	// XXX we assume vga is always available
	v_Video = new VGA;
	KASSERT(v_Video != nullptr, "no video attached?");

	for(auto& vtty: vttys) {
		Ananas::CreateDeviceProperties cdp(*this, Ananas::ResourceSet());
		vtty = new VTTY(cdp, *v_Video);
		if (auto result = vtty->Attach(); result.IsFailure())
			panic("cannot attach vtty!?");
	}
	activeVTTY = vttys.front();

	keyboard_mux::RegisterConsumer(*this);
	return Result::Success();
}

Result
VConsole::Detach()
{
	panic("TODO");

	for(auto& vtty: vttys) {
		if (auto result = vtty->Detach(); result.IsFailure())
			panic("cannot detach vtty!?");
	}
	keyboard_mux::UnregisterConsumer(*this);
	delete v_Video;
	return Result::Success();
}

Result
VConsole::Read(void* buf, size_t& len, off_t offset)
{
	return activeVTTY->Read(buf, len, offset);
}

Result
VConsole::Write(const void* buf, size_t& len, off_t offset)
{
	return activeVTTY->Write(buf, len, offset);
}

void
VConsole::OnKey(const keyboard_mux::Key& key)
{
	char ch = key.ch;
	switch(key.type) {
		case keyboard_mux::Key::Type::Character:
			activeVTTY->OnInput(&ch, 1);
			break;
		case keyboard_mux::Key::Type::Control:
			if (ch >= 'a' && ch <= 'z') {
				ch = (ch - 'a') + 1; // control-a => 1, etc
				activeVTTY->OnInput(&ch, 1);
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

} // unnamed namespace

REGISTER_DRIVER(VConsole_Driver)

/* vim:set ts=2 sw=2: */
