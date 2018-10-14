#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/result.h"

namespace driver_manager {

namespace {
static DriverList driverList; /* XXX not locked yet */
} // unnamed namespace

namespace internal {

DriverList& GetDriverList()
{
	return driverList;
}

} // namespace internal

Result Register(Driver& driver)
{
	// Insert the driver in-place - we want to keep the driver list sorted on
	// priority as this simplifies things (we can just look for the first driver
	// that matches)
	for(auto& d: driverList) {
		if (d.d_Priority < driver.d_Priority)
			continue;
		driverList.insert(d, driver);
		return Result::Success();
	}

	driverList.push_back(driver);
	return Result::Success();
}

Result Unregister(const char* name)
{
	for(auto& d: driverList) {
		if (strcmp(d.d_Name, name) != 0)
			continue;

		driverList.remove(d);
		return Result::Success();
	}
	return Result::Success();
}

} // namespace drivermanager

/* vim:set ts=2 sw=2: */
