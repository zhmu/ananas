#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/trace.h"

TRACE_SETUP;

namespace {

class CoreBus : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	CoreBus()
	{
		strcpy(d_Name, "corebus");
		d_Unit = 0;
	}

	IDeviceOperations& GetDeviceOperations() override {
		return *this;
	}

	errorcode_t Attach() override
	{
		return ananas_success();
	}

	errorcode_t Detach() override
	{
		return ananas_success();
	}

	void DebugDump() override
	{
	}

	errorcode_t DeviceControl(Process& proc, unsigned int op, void* buffer, size_t len) override
	{
		return ANANAS_ERROR(BAD_OPERATION);
	}
};

errorcode_t
Initialize()
{
	return Ananas::DeviceManager::AttachSingle(*new CoreBus);
}

INIT_FUNCTION(Initialize, SUBSYSTEM_DEVICE, ORDER_LAST);

} // unnamed namespace

/* vim:set ts=2 sw=2: */
