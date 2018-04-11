#include <ananas/types.h>
#include "kernel/console-driver.h"
#include "kernel/device.h"
#include "kernel/lib.h"

namespace {

struct VConsole : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations
{
	using Device::Device;
	virtual ~VConsole() = default;

	IDeviceOperations& GetDeviceOperations() override {
		return *this;
	}

	ICharDeviceOperations* GetCharDeviceOperations() override {
		return this;
	}

	Result Attach() override;
	Result Detach() override;
	Result Write(const void* buffer, size_t& len, off_t offset) override;

	Device* v_VGA = nullptr;
};

Result
VConsole::Attach()
{
	// XXX we assume vga is always available
	v_VGA = Ananas::DeviceManager::AttachChild(*this, d_ResourceSet);
	KASSERT(v_VGA != nullptr, "no vga attached?");
	return Result::Success();
}

Result
VConsole::Detach()
{
	panic("TODO");
	return Result::Success();
}

Result
VConsole::Write(const void* buffer, size_t& len, off_t offset)
{
	return v_VGA->GetCharDeviceOperations()->Write(buffer, len, offset);
}

struct VConsole_Driver : public Ananas::ConsoleDriver
{
	VConsole_Driver()
	 : ConsoleDriver("vconsole", 100, CONSOLE_FLAG_INOUT)
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
