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

namespace Ananas {

namespace USB {
class USBDevice;
class Transfer;
}

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
	virtual errorcode_t SetupTransfer(USB::Transfer&) = 0;
	virtual errorcode_t ScheduleTransfer(USB::Transfer&) = 0;
	virtual errorcode_t CancelTransfer(USB::Transfer&) = 0;
	virtual errorcode_t TearDownTransfer(USB::Transfer&) = 0;
	virtual void SetRootHub(USB::USBDevice&) = 0;
};

class IUSBHubDeviceOperations {
public:
	virtual void HandleExplore() = 0;
};

class Device;

struct CreateDeviceProperties
{
	CreateDeviceProperties(Ananas::Device& parent, const Ananas::ResourceSet& resourceSet)
	 : cdp_Parent(&parent), cdp_ResourceSet(resourceSet)
	{
	}

	CreateDeviceProperties(const Ananas::ResourceSet& resourceSet)
	 : cdp_ResourceSet(resourceSet)
	{
	}

	Ananas::Device* cdp_Parent = nullptr;
	Ananas::ResourceSet cdp_ResourceSet;
};

class Device {
public:
	Device(const CreateDeviceProperties& properties);
	Device();
	virtual ~Device();

	virtual IDeviceOperations& GetDeviceOperations() = 0;

	virtual ICharDeviceOperations* GetCharDeviceOperations() { return nullptr; }
	virtual IBIODeviceOperations* GetBIODeviceOperations() { return nullptr; }
	virtual IUSBDeviceOperations* GetUSBDeviceOperations() { return nullptr; }
	virtual IUSBHubDeviceOperations* GetUSBHubDeviceOperations() { return nullptr; }

	void Printf(const char* fmt, ...) const;

	// XXX Private me
	LIST_FIELDS(Device);
	Device* d_Parent = nullptr;
	char d_Name[64] = "unknown";
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

namespace DeviceManager {

errorcode_t AttachSingle(Device& device);
errorcode_t Detach(Device& device);
Device* AttachChild(Device& bus, const Ananas::ResourceSet& resourceSet);
void AttachBus(Device& bus);
Device* FindDevice(const char* name);
Device* CreateDevice(const char* driver, const Ananas::CreateDeviceProperties& cdp);

} // namespace DeviceManager

} // namespace Ananas

// Device-specific resource management; assigns the resource to the device
void* operator new(size_t len, device_t dev) throw();
void operator delete(void* p, device_t dev) throw();
void* operator new[](size_t len, device_t dev) throw();
void operator delete[](void* p, device_t dev) throw();

#endif /* __DEVICE_H__ */
