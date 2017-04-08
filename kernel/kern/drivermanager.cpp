#include <ananas/driver.h>
#include <ananas/lib.h>

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
	LIST_FOREACH_SAFE(&driverList, d, Driver) {
		if (d->d_Priority < driver.d_Priority)
			continue;
		LIST_INSERT_BEFORE(&driverList, d, &driver);
		return ananas_success();
	}

	LIST_APPEND(&driverList, &driver);
	return ananas_success();
}

errorcode_t Unregister(const char* name)
{
	LIST_FOREACH_SAFE(&driverList, d, Driver) {
		if (strcmp(d->d_Name, name) != 0)
			continue;

		LIST_REMOVE(&driverList, d);
		return ananas_success();
	}
	return ananas_success();
}

} // namespace DriverManager
} // namespace Ananas

/* vim:set ts=2 sw=2: */
