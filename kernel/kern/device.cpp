#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/kdb.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/kmem.h>
#include <ananas/tty.h>
#include <ananas/thread.h>
#include <ananas/trace.h>
#include <ananas/vm.h>
#include "options.h"

TRACE_SETUP;

static spinlock_t spl_devicequeue = SPINLOCK_DEFAULT_INIT;
static Ananas::DeviceList deviceList;
static struct Ananas::DeviceProbeList deviceProbeList; /* XXX not locked yet */

namespace Ananas {
namespace DeviceManager {
namespace internal {
void Unregister(Device&);
} // namespace internal
// namespace DeviceManager
}

CreateDeviceProperties::CreateDeviceProperties(Ananas::Device& parent, const DeviceProbe& p, const Ananas::ResourceSet& resourceSet)
	: cdp_Parent(&parent), cdp_ResourceSet(resourceSet)
{
	cdp_Name = p.dp_Name;
	cdp_Unit = p.dp_CurrentUnit;
}

Device::Device(const CreateDeviceProperties& cdp)
	: d_Parent(cdp.cdp_Parent),
	  d_ResourceSet(cdp.cdp_ResourceSet)
{
	li_next = NULL; li_prev = NULL;
	sem_init(&d_Waiters, 1);

	strcpy(d_Name, cdp.cdp_Name);
	d_Unit = cdp.cdp_Unit;
}

Device::~Device()
{
	DeviceManager::internal::Unregister(*this);
}

void Device::Printf(const char* fmt, ...) const
{
#define DEVICE_PRINTF_BUFSIZE 256
	char buf[DEVICE_PRINTF_BUFSIZE];

	snprintf(buf, sizeof(buf), "%s%u: ", d_Name, d_Unit);

	va_list va;
	va_start(va, fmt);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 2, fmt, va);
	buf[sizeof(buf) - 2] = '\0';
	strcat(buf, "\n");
	va_end(va);

	console_putstring(buf);
}

namespace DeviceManager {

namespace {

Device* CreateDevice(Device& bus, DeviceProbe& dp, const Ananas::ResourceSet& resourceSet)
{
	return dp.dp_CreateDeviceFunc(CreateDeviceProperties(bus, dp, resourceSet));
}

void PrintAttachment(Device& device)
{
	KASSERT(device.d_Parent != NULL, "can't print device which doesn't have a parent bus");
	kprintf("%s%u on %s%u ", device.d_Name, device.d_Unit, device.d_Parent->d_Name, device.d_Parent->d_Unit);
	device.d_ResourceSet.Print();
	kprintf("\n");
}

bool
MustProbeOnBus(const Ananas::DeviceProbe& p, const Device& bus)
{
	const char* ptr = p.dp_Busses;
	while(*ptr != '\0') {
		const char* next = strchr(ptr, ',');
		if (next == NULL)
			next = strchr(ptr, '\0');

		if (strncmp(ptr, bus.d_Name, next - ptr) == 0)
			return true;

		ptr = next + 1;
	}
	return false;
}

} // unnamed namespace

namespace internal {

void Register(Device& device)
{
	spinlock_lock(&spl_devicequeue);
	LIST_APPEND(&deviceList, &device);
	spinlock_unlock(&spl_devicequeue);
}

void Unregister(Device& device)
{
	/* XXX clear waiters; should we signal them? */

	spinlock_lock(&spl_devicequeue);
	LIST_REMOVE(&deviceList, &device);
	spinlock_unlock(&spl_devicequeue);
}

} // namespace internal

errorcode_t
AttachSingle(Device& device)
{
#if 0
	/*
	 * This device has a probe function; we must call it to figure out
 	 * whether the device actually exists or we're about to attach
	 * something out of thin air here...
	 */
	errorcode_t err = driver->drv_probe(dev->d_resourceset);
	ANANAS_ERROR_RETURN(err);
#endif

	/*
	 * XXX This is a kludge: it prevents us from displaying attach information for drivers
	 * that will be initialize outside the probe tree (such as the console which will be
	 * initialized as soon as possible.
	 */
	if (device.d_Parent != nullptr)
		PrintAttachment(device);

	errorcode_t err = device.GetDeviceOperations().Attach();
	ANANAS_ERROR_RETURN(err);

	/* Hook the device up to the tree */
	internal::Register(device);

	/* Attempt to attach child devices, if any */
#if 0
	if (driver->drv_attach_children != NULL)
		driver->drv_attach_children(dev);
#endif
	AttachBus(device);

	return ananas_success();
}

/*
 * Attaches a single device, by handing it off to all possible drivers (first
 * one wins). Succeeds if a driver accepted the device, in which case the
 * device name and unit will be updated.
 */
errorcode_t
AttachChild(Device& bus, const Ananas::ResourceSet& resourceSet)
{
	if (LIST_EMPTY(&deviceProbeList))
		return ANANAS_ERROR(NO_DEVICE);

	LIST_FOREACH_SAFE(&deviceProbeList, dp, DeviceProbe) {
		if (!MustProbeOnBus(*dp, bus))
			continue; /* bus cannot contain this device */

		/* Hook the device to this driver and try to attach it */
		Device* device = CreateDevice(bus, *dp, resourceSet);
		if (device == NULL)
			continue;
		if (ananas_is_success(AttachSingle(*device))) {
			return ananas_success();
		}

		// Attach failed - get rid of the device
		delete device;
	}

	return ANANAS_ERROR(NO_DEVICE);
}

/*
 * Attaches children on a given bus - assumes the bus is already set up. May
 * attach multiple devices or none at all.
 *
 * XXX contains hacks for the input/output console device stuff (skips
 * attaching them, as they are already at the point this is run)
 */
void
AttachBus(Device& bus)
{
	if (LIST_EMPTY(&deviceProbeList))
		return;

	/*
	 * Fetch TTY devices; we need them to report the device that is already
	 * attached at this point.
	 */
#if 0
	Ananas::Device* input_dev = tty_get_inputdev(console_tty);
	Ananas::Device* output_dev = tty_get_outputdev(console_tty);
#endif
	LIST_FOREACH_SAFE(&deviceProbeList, dp, DeviceProbe) {
		if (!MustProbeOnBus(*dp, bus))
			continue; /* bus cannot contain this device */

		/*
		 * If we found the driver for the in- or output driver, display it.
		 *
		 * XXX we will any extra units here, maybe we should re-think that? The entire
		 *     tty-device-stuff is a hack anyway...
		 */
#if 0
		if (input_dev != NULL && input_dev->driver == driver) {
			input_dev->parent = bus;
			device_print_attachment(input_dev);
			device_add_to_tree(input_dev);
			continue;
		}
		if (output_dev != NULL && output_dev->driver == driver && input_dev != output_dev) {
			output_dev->parent = bus;
			device_print_attachment(output_dev);
			device_add_to_tree(output_dev);
			continue;
		}
#endif

		Ananas::ResourceSet resourceSet; // TODO

		// See if the driver accepts our resource set
		Device* device = CreateDevice(bus, *dp, resourceSet);
		if (device == nullptr)
			continue;

		if (ananas_is_failure(AttachSingle(*device))) {
			delete device; // attach failed; discard the device
		}
	}
}

errorcode_t
RegisterDeviceProbe(DeviceProbe& p)
{
	LIST_APPEND(&deviceProbeList, &p);
	return ananas_success();
}

errorcode_t
UnregisterDeviceProbe(DeviceProbe& p)
{
	LIST_REMOVE(&deviceProbeList, &p);
	return ananas_success();
}

namespace {

class CoreBus : public Device, private IDeviceOperations
{
public:
	using Device::Device;

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

	errorcode_t DeviceControl(process_t* proc, unsigned int op, void* buffer, size_t len) override
	{
		return ANANAS_ERROR(BAD_OPERATION);
	}
};

} // unnamed namespace

Device*
FindDevice(const char* name)
{
	char* ptr = (char*)name;
	while (*ptr != '\0' && (*ptr < '0' || *ptr > '9'))
		ptr++;
	int unit = (*ptr != '\0') ? strtoul(ptr, NULL, 10) : 0;

	LIST_FOREACH(&deviceList, device, Device) {
		if (!strncmp(device->d_Name, name, ptr - name) && device->d_Unit == unit)
			return device;
	}
	return nullptr;
}

errorcode_t
Initialize()
{
	LIST_INIT(&deviceList);

	Device* corebus = new CoreBus(CreateDeviceProperties("corebus", 0));
	return AttachSingle(*corebus);
}

INIT_FUNCTION(Initialize, SUBSYSTEM_DEVICE, ORDER_LAST);

} // namespace DeviceManager

} // namespace Ananas

void*
operator new(size_t len, device_t dev) throw()
{
	return kmalloc(len);
}

void*
operator new[](size_t len, device_t dev) throw()
{
	return kmalloc(len);
}

void
operator delete(void* p, device_t dev) throw()
{
	kfree(p);
}

void
operator delete[](void* p, device_t dev) throw()
{
	kfree(p);
}

#ifdef OPTION_KDB
static int
print_devices(Ananas::Device* parent, int indent)
{
	int count = 0;
	LIST_FOREACH(&deviceList, dev, Ananas::Device) {
		if (dev->d_Parent != parent)
			continue;
		for (int n = 0; n < indent; n++)
			kprintf(" ");
		kprintf("device %p: '%s' unit %u\n",
	 	 dev, dev->d_Name, dev->d_Unit);
		count += print_devices(dev, indent + 1) + 1;
	}
	return count;
}

KDB_COMMAND(devices, NULL, "Displays a list of all devices")
{
	int count = print_devices(NULL, 0);

	/* See if we have printed everything; if not, our structure is wrong and we should fix this */
	int n = 0;
	LIST_FOREACH(&deviceList, dev, Ananas::Device) {
		n++;
	}
	if (n != count)
		kprintf("Warning: %d extra device(s) unreachable by walking the device chain!\n", n - count);
}

KDB_COMMAND(devdump, "s:devname", "Displays debugging dump of a device")
{
	auto dev = Ananas::DeviceManager::FindDevice(arg[1].a_u.u_string);
	if (dev == nullptr) {
		kprintf("device not found\n");
		return;
	}

	dev->GetDeviceOperations().DebugDump();
}
#endif

/* vim:set ts=2 sw=2: */
