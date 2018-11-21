#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <ananas/util/list.h>
#include "kernel/init.h"

class Result;

struct CreateDeviceProperties;
class Device;
class Driver;
class ConsoleDriver;

namespace driver_manager {

Result Register(Driver& driver);
Result Unregister(const char* name);

} // namespace driver_manager

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

	virtual Device* CreateDevice(const CreateDeviceProperties& cdp) = 0;
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

template<typename T>
struct RegisterDriver : init::OnInit
{
	RegisterDriver() : OnInit(init::SubSystem::Driver, init::Order::Middle, []() {
		driver_manager::Register(*new T);
	}) { }
};

typedef util::List<Driver> DriverList;

#endif /* __DRIVER_H__ */
