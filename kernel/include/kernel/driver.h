#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <ananas/util/list.h>
#include "kernel/init.h"

class Result;

namespace Ananas {

struct CreateDeviceProperties;
class Device;
class Driver;
class ConsoleDriver;

namespace DriverManager {

Result Register(Driver& driver);
Result Unregister(const char* name);

} // namespace DriverManager

/*
 * Driver has three main purposes:
 *
 * - Take care of the driver name and unit assignment
 * - Create a given Device object, either by probing or on request
 * - Determine on which busses the device can occur
 */
class Driver : public util::List<Driver>::NodePtr {
public:
	Driver(const char* name, int priority = 1000)
	 : d_Name(name), d_Priority(priority)
	{
	}
	virtual ~Driver() = default;

	virtual Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) = 0;
	virtual const char* GetBussesToProbeOn() const = 0;

	virtual ConsoleDriver* GetConsoleDriver()
	{
		return nullptr;
	}

	bool MustProbeOnBus(const Device& bus) const;

	const char* d_Name;
	int d_Priority;
	int d_Major = 0;
	int d_CurrentUnit = 0;
};

typedef util::List<Driver> DriverList;

// XXX The Unregister-part is a bit clumsy...
#define REGISTER_DRIVER(name) \
	static Result register_driver_##name() { \
		return Ananas::DriverManager::Register(*new name); \
	} \
	static Result unregister_driver_##name() { \
		Ananas::Driver* d = new name; \
		auto result = Ananas::DriverManager::Unregister(d->d_Name); \
		delete d; \
		return result; \
	} \
	INIT_FUNCTION(register_driver_##name, SUBSYSTEM_DRIVER, ORDER_MIDDLE); \
	EXIT_FUNCTION(unregister_driver_##name);

} // namespace Ananas

#endif /* __DRIVER_H__ */
