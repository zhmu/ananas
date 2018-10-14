#include "kernel/console.h"
#include "kernel/console-driver.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/vfs/mount.h" // for vfs_abandon_device()
#include "kernel/vm.h"
#include "options.h"

namespace driver_manager::internal {
DriverList& GetDriverList();
} // namespace driver_manager::internal

namespace device_manager {

namespace {

int currentMajor = 1;

void PrintAttachment(Device& device)
{
	KASSERT(device.d_Parent != NULL, "can't print device which doesn't have a parent bus");
	kprintf("%s%u on %s%u ", device.d_Name, device.d_Unit, device.d_Parent->d_Name, device.d_Parent->d_Unit);
	device.d_ResourceSet.Print();
	kprintf("\n");
}

} // unnamed namespace

namespace internal {

Spinlock spl_devicequeue;
DeviceList deviceList;

void Register(Device& device)
{
	SpinlockGuard g(spl_devicequeue);
	for(auto& d: deviceList) {
		KASSERT(&d != &device, "registering device '%s' already in the device queue", device.d_Name);
	}
	deviceList.push_back(device);
}

void OnDeviceDestruction(Device& device)
{
	// This is just a safety precaution for now
	SpinlockGuard g(spl_devicequeue);
	for(auto& d: deviceList) {
		KASSERT(&d != &device, "destroying device '%s' still in the device queue", device.d_Name);
	}
}

void Unregister(Device& device)
{
	/* XXX clear waiters; should we signal them? */

	SpinlockGuard g(spl_devicequeue);
	deviceList.remove(device);
}

Device* InstantiateDevice(Driver& driver, const CreateDeviceProperties& cdp)
{
	// XXX we need locking for currentunit / major / currentMajor
	if (driver.d_Major == 0)
		driver.d_Major = currentMajor++;

	Device* device = driver.CreateDevice(cdp);
	if (device != nullptr) {
		strcpy(device->d_Name, driver.d_Name);
		device->d_Major = driver.d_Major;
		device->d_Unit = driver.d_CurrentUnit++;
	}
	return device;
}

Device* ProbeConsole(ConsoleDriver& driver)
{
	Device* device = driver.ProbeDevice();
	if (device == nullptr)
		return nullptr;

	// XXX duplication, locking etc
	if (driver.d_Major == 0)
		driver.d_Major = currentMajor++;
	strcpy(device->d_Name, driver.d_Name);
	device->d_Major = driver.d_Major;
	device->d_Unit = driver.d_CurrentUnit++;
	return device;
}

void DeinstantiateDevice(Device& device)
{
	delete &device;
}

} // namespace internal

Result
AttachSingle(Device& device)
{
	/*
	 * XXX This is a kludge: it prevents us from displaying attach information for drivers
	 * that will be initialize outside the probe tree (such as the console which will be
	 * initialized as soon as possible.
	 */
	if (device.d_Parent != nullptr)
		PrintAttachment(device);

	if (auto result = device.GetDeviceOperations().Attach(); result.IsFailure())
		return result;

	/* Hook the device up to the tree */
	internal::Register(device);
	if (device.d_Parent != nullptr)
		device.d_Parent->d_Children.push_back(device);

	/* Attempt to attach child devices, if any */
	AttachBus(device);

	return Result::Success();
}

Result
Detach(Device& device)
{
	// Ensure the device is out of the queue; this prevents new access to the
	// device device
	internal::Unregister(device);

	// In case there is any filesystem here, abandon it
	vfs_abandon_device(device);

	// All children must be detached before we can clean up the main device
	for(auto it = device.d_Children.begin(); it != device.d_Children.end(); /* nothing */) {
		Device& childDevice = *it; ++it;
		Detach(childDevice);
	}

	// XXX I wonder how realistic this is - failing to detach (what can we do?)
	if (auto result = device.GetDeviceOperations().Detach(); result.IsFailure())
		return result;

	device.Printf("detached");

	// XXX We should destroy the device, but only if we know this is safe (we need refcounting here)
	internal::DeinstantiateDevice(device);
	return Result::Success();
}

/*
 * Attaches a single device, by handing it off to all possible drivers (first
 * one wins). Succeeds if a driver accepted the device, in which case the
 * device name and unit will be updated.
 */
Device*
AttachChild(Device& bus, const ResourceSet& resourceSet)
{
	auto& driverList = driver_manager::internal::GetDriverList();
	if (driverList.empty())
		return nullptr;

	CreateDeviceProperties cdp(bus, resourceSet);
	for(auto it = driverList.begin(); it != driverList.end(); /* nothing */) {
		Driver& d = *it; ++it;
		if (!d.MustProbeOnBus(bus))
			continue; /* bus cannot contain this device */

		/* Hook the device to this driver and try to attach it */
		Device* device = internal::InstantiateDevice(d, cdp);
		if (device == nullptr)
			continue;
		if (auto result = AttachSingle(*device); result.IsSuccess())
			return device;

		// Attach failed - get rid of the device
		internal::DeinstantiateDevice(*device);
	}

	return nullptr;
}

/*
 * Attaches children on a given bus - assumes the bus is already set up. May
 * attach multiple devices or none at all.
 *
 * Note that no special care is done to prevent double attachment of the
 * console (which is already set up at this point) - it is up to the device
 * to handle this and prevent attaching.
 */
void
AttachBus(Device& bus)
{
	auto& driverList = driver_manager::internal::GetDriverList();
	if (driverList.empty())
		return;

	for(auto it = driverList.begin(); it != driverList.end(); /* nothing */) {
		Driver& d = *it; ++it;
		if (!d.MustProbeOnBus(bus))
			continue; /* bus cannot contain this device */

		ResourceSet resourceSet; // TODO

		// See if the driver accepts our resource set
		Device* device = internal::InstantiateDevice(d, CreateDeviceProperties(bus, resourceSet));
		if (device == nullptr)
			continue;

		if (auto result = AttachSingle(*device); result.IsFailure()) {
			internal::DeinstantiateDevice(*device); // attach failed; discard the device
		}
	}
}

Device*
FindDevice(const char* name)
{
	char* ptr = (char*)name;
	while (*ptr != '\0' && (*ptr < '0' || *ptr > '9'))
		ptr++;
	int unit = (*ptr != '\0') ? strtoul(ptr, NULL, 10) : 0;

	SpinlockGuard g(internal::spl_devicequeue);
	for(auto& device: internal::deviceList) {
		if (strncmp(device.d_Name, name, ptr - name) == 0 && device.d_Unit == unit)
			return &device;
	}
	return nullptr;
}

Device*
FindDevice(dev_t dev)
{
	SpinlockGuard g(internal::spl_devicequeue);

	for(auto& device: internal::deviceList) {
		if (device.d_Major == major(dev) && device.d_Unit == minor(dev))
			return &device;
	}
	return nullptr;
}

Device*
CreateDevice(const char* driver, const CreateDeviceProperties& cdp)
{
	auto& driverList = driver_manager::internal::GetDriverList();
	for(auto it = driverList.begin(); it != driverList.end(); /* nothing */) {
		Driver& d = *it; ++it;
		if (strcmp(d.d_Name, driver) == 0)
			return internal::InstantiateDevice(d, cdp);
	}

	return nullptr;
}

} // namespace device_manager

#ifdef OPTION_KDB
static int
print_devices(Device* parent, int indent)
{
	int count = 0;
	for(auto& dev: device_manager::internal::deviceList) {
		if (dev.d_Parent != parent)
			continue;
		for (int n = 0; n < indent; n++)
			kprintf(" ");
		kprintf("device %p: '%s' unit %u\n",
		 &dev, dev.d_Name, dev.d_Unit);
		count += print_devices(&dev, indent + 1) + 1;
	}
	return count;
}

KDB_COMMAND(devices, NULL, "Displays a list of all devices")
{
	int count = print_devices(NULL, 0);

	/* See if we have printed everything; if not, our structure is wrong and we should fix this */
	int n = 0;
	for(auto& dev: device_manager::internal::deviceList) {
		n++;
	}
	if (n != count)
		kprintf("Warning: %d extra device(s) unreachable by walking the device chain!\n", n - count);
}

KDB_COMMAND(devdump, "s:devname", "Displays debugging dump of a device")
{
	auto dev = device_manager::FindDevice(arg[1].a_u.u_string);
	if (dev == nullptr) {
		kprintf("device not found\n");
		return;
	}

	dev->GetDeviceOperations().DebugDump();
}
#endif

/* vim:set ts=2 sw=2: */
