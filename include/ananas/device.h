#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/list.h>
#include <ananas/lock.h>
#include <ananas/init.h>
#include <ananas/cdefs.h>
#include <ananas/resourceset.h>

typedef struct DEVICE* device_t;
typedef struct DRIVER* driver_t;
typedef struct PROBE* probe_t;

struct BIO;
struct USB_BUS;
struct USB_DEVICE;
struct USB_TRANSFER;

namespace Ananas {

class IDeviceOperations {
public:
	virtual errorcode_t Attach() = 0;
	virtual errorcode_t Detach() = 0;
	virtual void DebugDump() { }

	virtual errorcode_t DeviceControl(process_t* proc, unsigned int op, void* buffer, size_t len)
	{
		return ananas_make_error(ANANAS_ERROR_UNSUPPORTED);
	}
};

class ICharDeviceOperations {
public:
	virtual errorcode_t Write(const void* buffer, size_t& len, off_t offset)
	{
		return ananas_make_error(ANANAS_ERROR_UNSUPPORTED);
	}

	virtual errorcode_t Read(void* buffer, size_t& len, off_t offset)
	{
		return ananas_make_error(ANANAS_ERROR_UNSUPPORTED);
	}
};

class IBIODeviceOperations {
public:
	virtual errorcode_t ReadBIO(struct BIO& bio) = 0;
	virtual errorcode_t WriteBIO(struct BIO& bio) = 0;
};

class IUSBDeviceOperations {
public:
	virtual errorcode_t SetupTransfer(struct USB_TRANSFER*) = 0;
	virtual errorcode_t ScheduleTransfer(struct USB_TRANSFER*) = 0;
	virtual errorcode_t CancelTransfer(struct USB_TRANSFER*) = 0;
	virtual errorcode_t TearDownTransfer(struct USB_TRANSFER*) = 0;
	virtual void* InitializeHCDPrivateData(int) = 0;
	virtual void SetRootHub(struct USB_DEVICE*) = 0;
	virtual errorcode_t RootHubTransfer(struct USB_TRANSFER*) = 0;
	virtual void Explore(struct USB_DEVICE*) = 0;
};

class Device;
struct DeviceProbe;

struct CreateDeviceProperties
{
	CreateDeviceProperties(Ananas::Device& parent, const DeviceProbe& p, const Ananas::ResourceSet& resourceSet);
	CreateDeviceProperties(Ananas::Device& parent, const char* name, int unit, const Ananas::ResourceSet& resourceSet = Ananas::ResourceSet())
	 : cdp_Parent(&parent), cdp_Name(name), cdp_Unit(unit), cdp_ResourceSet(resourceSet)
	{
	}
	CreateDeviceProperties(const char* name, int unit, const Ananas::ResourceSet& resourceSet = Ananas::ResourceSet())
	 : cdp_Parent(nullptr), cdp_Name(name), cdp_Unit(unit), cdp_ResourceSet(resourceSet)
	{
	}

	Ananas::Device* cdp_Parent = nullptr;
	const char* cdp_Name = "???";
	int cdp_Unit = -1;
	Ananas::ResourceSet cdp_ResourceSet;
};

class Device {
public:
	Device(const CreateDeviceProperties& properties);
	virtual ~Device();

	virtual IDeviceOperations& GetDeviceOperations() = 0;

	virtual ICharDeviceOperations* GetCharDeviceOperations() { return nullptr; }
	virtual IBIODeviceOperations* GetBIODeviceOperations() { return nullptr; }
	virtual IUSBDeviceOperations* GetUSBDeviceOperations() { return nullptr; }

	void Printf(const char* fmt, ...) const;

	// XXX Private me
	LIST_FIELDS(Device);
	Device* d_Parent = nullptr;
	char d_Name[64]; // XXX
	unsigned int d_Unit = -1;
	ResourceSet d_ResourceSet;
	dma_tag_t d_DMA_tag;
	semaphore_t d_Waiters;

	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
};
LIST_DEFINE(DeviceList, Device);

// Create device function, used while probing
typedef Device* (*CreateDeviceFunc)(const Ananas::CreateDeviceProperties& cdp);

/*
 * The Device Probe structure describes a possible relationship between a bus
 * and a device (i.e. device X may appear on bus Y). Once a bus is attaching,
 * it will use these declarations as an attempt to add all possible devices on
 * the bus.
 */
struct DeviceProbe {
	const char* dp_Name;
	int dp_CurrentUnit;
	CreateDeviceFunc dp_CreateDeviceFunc;
	LIST_FIELDS(DeviceProbe);

	/* Busses this device appears on */
	const char* dp_Busses;
};
LIST_DEFINE(DeviceProbeList, DeviceProbe);

namespace DeviceManager {

errorcode_t AttachSingle(Device& device);
errorcode_t AttachChild(Device& bus, const Ananas::ResourceSet& resourceSet);
void AttachBus(Device& bus);
Device* FindDevice(const char* name);

errorcode_t RegisterDeviceProbe(DeviceProbe& p);
errorcode_t UnregisterDeviceProbe(DeviceProbe& p);
} // namespace DeviceManager

} // namespace Ananas

#define DRIVER_PROBE(id, name, create_dev_func) \
	extern Ananas::DeviceProbe probe_##id; \
	static errorcode_t register_##id() { \
		return Ananas::DeviceManager::RegisterDeviceProbe(probe_##id); \
	}; \
	static errorcode_t unregister_##id() { \
		return Ananas::DeviceManager::UnregisterDeviceProbe(probe_##id); \
	}; \
	INIT_FUNCTION(register_##id, SUBSYSTEM_DEVICE, ORDER_MIDDLE); \
	EXIT_FUNCTION(unregister_##id); \
	Ananas::DeviceProbe probe_##id= { \
		.dp_Name = (name), \
		.dp_CurrentUnit = 0, \
		.dp_CreateDeviceFunc = &create_dev_func, \
		.dp_Busses =

#define DRIVER_PROBE_BUS(bus) \
			STRINGIFY(bus) ","

#define DRIVER_PROBE_END() \
	};

/*
void device_waiter_add_head(device_t dev, struct THREAD* thread);
void device_waiter_add_tail(device_t dev, struct THREAD* thread);
void device_waiter_signal(device_t dev);
*/

// Device-specific resource management; assigns the resource to the device
void* operator new(size_t len, device_t dev) throw();
void operator delete(void* p, device_t dev) throw();
void* operator new[](size_t len, device_t dev) throw();
void operator delete[](void* p, device_t dev) throw();

#endif /* __DEVICE_H__ */
