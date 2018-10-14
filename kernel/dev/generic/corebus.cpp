#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"

TRACE_SETUP;

namespace {

class CoreBus : public Device, private IDeviceOperations
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

	Result Attach() override
	{
		return Result::Success();
	}

	Result Detach() override
	{
		return Result::Success();
	}

	void DebugDump() override
	{
	}
};

Result
Initialize()
{
	return device_manager::AttachSingle(*new CoreBus);
}

INIT_FUNCTION(Initialize, SUBSYSTEM_DEVICE, ORDER_LAST);

} // unnamed namespace

/* vim:set ts=2 sw=2: */
