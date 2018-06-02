#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/util/list.h>
#include "kernel/lock.h"
#include "kernel/result.h"
#include "kernel/resourceset.h"

typedef struct DEVICE* device_t;
typedef struct DRIVER* driver_t;
typedef struct PROBE* probe_t;

struct BIO;
struct Process;

namespace Ananas {
class Device;
}

namespace device {

// Internal stuff so we can work with children and all nodes
namespace internal {

template<typename T>
struct AllNode
{
	static typename util::List<T>::Node& Get(T& t) {
		return t.p_NodeAll;
	}
};

template<typename T>
struct ChildrenNode
{
	static typename util::List<T>::Node& Get(T& t) {
		return t.p_NodeChildren;
	}
};

template<typename T> using DeviceAllNodeAccessor = typename util::List<T>::template nodeptr_accessor<AllNode<T> >;
template<typename T> using DeviceChildrenNodeAccessor = typename util::List<T>::template nodeptr_accessor<ChildrenNode<T> >;

} // namespace internal

typedef util::List<::Ananas::Device, internal::DeviceAllNodeAccessor<::Ananas::Device> > ChildrenList;
typedef util::List<::Ananas::Device, internal::DeviceChildrenNodeAccessor<::Ananas::Device> > DeviceList;

} // namespace device

namespace Ananas {

namespace USB {
class USBDevice;
class Transfer;
}

class IDeviceOperations {
public:
	virtual Result Attach() = 0;
	virtual Result Detach() = 0;
	virtual void DebugDump() { }

	virtual Result Open(Process* proc)
	{
		return Result::Success();
	}

	virtual Result Close(Process* proc)
	{
		return Result::Success();
	}

	virtual Result IOControl(Process* proc, unsigned long request, void* args[])
	{
		return Result::FromErrNo(EINVAL);
	}
};

class ICharDeviceOperations {
public:
	virtual Result Write(const void* buffer, size_t len, off_t offset)
	{
		return Result::FromErrNo(EINVAL);
	}

	virtual Result Read(void* buffer, size_t len, off_t offset)
	{
		return Result::FromErrNo(EINVAL);
	}
};

class IBIODeviceOperations {
public:
	virtual Result ReadBIO(struct BIO& bio) = 0;
	virtual Result WriteBIO(struct BIO& bio) = 0;
};

class IUSBDeviceOperations {
public:
	virtual Result SetupTransfer(USB::Transfer&) = 0;
	virtual Result ScheduleTransfer(USB::Transfer&) = 0;
	virtual Result CancelTransfer(USB::Transfer&) = 0;
	virtual Result TearDownTransfer(USB::Transfer&) = 0;
	virtual void SetRootHub(USB::USBDevice&) = 0;
};

class IUSBHubDeviceOperations {
public:
	virtual void HandleExplore() = 0;
};

class ISCSIDeviceOperations
{
public:
	enum class Direction {
		D_In,
		D_Out
	};

	virtual Result PerformSCSIRequest(int lun, Direction dir, const void* cb, size_t cb_len, void* result, size_t* result_len) = 0;
};

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
	virtual ISCSIDeviceOperations* GetSCSIDeviceOperations() { return nullptr; }

	void Printf(const char* fmt, ...) const;

	// XXX Private me
	util::List<Device>::Node p_NodeAll;
	util::List<Device>::Node p_NodeChildren;
	device::ChildrenList d_Children;

	Device* d_Parent = nullptr;
	char d_Name[64] = "unknown";
	unsigned int d_Major = -1;
	unsigned int d_Unit = -1;
	ResourceSet d_ResourceSet;
	dma_tag_t d_DMA_tag = nullptr;

	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
};

// Create device function, used while probing
typedef Device* (*CreateDeviceFunc)(const Ananas::CreateDeviceProperties& cdp);

namespace DeviceManager {

using DeviceList = ::device::DeviceList;

Result AttachSingle(Device& device);
Result Detach(Device& device);
Device* AttachChild(Device& bus, const Ananas::ResourceSet& resourceSet);
void AttachBus(Device& bus);
Device* FindDevice(const char* name);
Device* FindDevice(dev_t dev);
Device* CreateDevice(const char* driver, const Ananas::CreateDeviceProperties& cdp);

} // namespace DeviceManager

} // namespace Ananas

// Device-specific resource management; assigns the resource to the device
void* operator new(size_t len, device_t dev) throw();
void operator delete(void* p, device_t dev) throw();
void* operator new[](size_t len, device_t dev) throw();
void operator delete[](void* p, device_t dev) throw();

#endif /* __DEVICE_H__ */
