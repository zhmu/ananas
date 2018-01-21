#include "kernel/driver.h"
#include "kernel/lib.h"

namespace Ananas {

namespace DriverManager {

namespace {
static Ananas::DriverList driverList; /* XXX not locked yet */
} // unnamed namespace

namespace internal {

Ananas::DriverList& GetDriverList()
{
	return driverList;
}

} // namespace internal

errorcode_t Register(Driver& driver)
{
	// Insert the driver in-place - we want to keep the driver list sorted on
	// priority as this simplifies things (we can just look for the first driver
	// that matches)
	for(auto& d: driverList) {
		if (d.d_Priority < driver.d_Priority)
			continue;
		driverList.insert(d, driver);
		return ananas_success();
	}

	driverList.push_back(driver);
	return ananas_success();
}

errorcode_t Unregister(const char* name)
{
	for(auto& d: driverList) {
		if (strcmp(d.d_Name, name) != 0)
			continue;

		driverList.remove(d);
		return ananas_success();
	}
	return ananas_success();
}

} // namespace DriverManager
} // namespace Ananas

/* vim:set ts=2 sw=2: */
