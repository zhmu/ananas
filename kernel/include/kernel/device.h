/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/types.h"
#include <ananas/errno.h>
#include <ananas/util/list.h>
#include "kernel/lock.h"
#include "kernel/result.h"
#include "kernel/resourceset.h"

struct BIO;
struct Process;

class Device;

namespace irq
{
    class IIRQCallback;
}

namespace dma
{
    class Tag;
}

namespace device
{
    // Internal stuff so we can work with children and all nodes
    namespace internal
    {
        template<typename T>
        struct AllNode {
            static typename util::List<T>::Node& Get(T& t) { return t.p_NodeAll; }
        };

        template<typename T>
        struct ChildrenNode {
            static typename util::List<T>::Node& Get(T& t) { return t.p_NodeChildren; }
        };

        template<typename T>
        using DeviceAllNodeAccessor = typename util::List<T>::template nodeptr_accessor<AllNode<T>>;
        template<typename T>
        using DeviceChildrenNodeAccessor =
            typename util::List<T>::template nodeptr_accessor<ChildrenNode<T>>;

    } // namespace internal

    typedef util::List<::Device, internal::DeviceAllNodeAccessor<::Device>> ChildrenList;
    typedef util::List<::Device, internal::DeviceChildrenNodeAccessor<::Device>> DeviceList;

} // namespace device

namespace usb
{
    class USBDevice;
    class Transfer;
} // namespace usb

struct VFS_FILE;

class IDeviceOperations
{
  public:
    virtual Result Attach() = 0;
    virtual Result Detach() = 0;

    virtual Result Open(Process* proc) { return Result::Success(); }

    virtual Result Close(Process* proc) { return Result::Success(); }

    virtual Result IOControl(Process* proc, unsigned long request, void* args[])
    {
        return Result::Failure(EINVAL);
    }

    virtual Result DetermineDevicePhysicalAddres(addr_t& physAddress, size_t& length, int& mapFlags)
    {
        return Result::Failure(EINVAL);
    }
};

class ICharDeviceOperations
{
  public:
    virtual Result Write(VFS_FILE& file, const void* buffer, size_t len)
    {
        return Result::Failure(EINVAL);
    }

    virtual Result Read(VFS_FILE& file, void* buffer, size_t len)
    {
        return Result::Failure(EINVAL);
    }

    virtual bool CanRead()
    {
        return false;
    }
};

class IBIODeviceOperations
{
  public:
    virtual void ReadBIO(struct BIO& bio) = 0;
    virtual void WriteBIO(struct BIO& bio) = 0;
};

class IUSBDeviceOperations
{
  public:
    virtual Result SetupTransfer(usb::Transfer&) = 0;
    virtual Result ScheduleTransfer(usb::Transfer&) = 0;
    virtual Result CancelTransfer(usb::Transfer&) = 0;
    virtual Result TearDownTransfer(usb::Transfer&) = 0;
    virtual void SetRootHub(usb::USBDevice&) = 0;
};

class IUSBHubDeviceOperations
{
  public:
    virtual void HandleExplore() = 0;
};

class ISCSIDeviceOperations
{
  public:
    enum class Direction { D_In, D_Out };

    virtual Result PerformSCSIRequest(
        int lun, Direction dir, const void* cb, size_t cb_len, void* result,
        size_t* result_len) = 0;
};

class IBusOperations
{
  public:
    virtual Result AllocateIRQ(Device& device, int index, irq::IIRQCallback&) = 0;
};

struct CreateDeviceProperties {
    CreateDeviceProperties(Device& parent, const ResourceSet& resourceSet)
        : cdp_Parent(&parent), cdp_ResourceSet(resourceSet)
    {
    }

    CreateDeviceProperties(const ResourceSet& resourceSet) : cdp_ResourceSet(resourceSet) {}

    Device* cdp_Parent = nullptr;
    ResourceSet cdp_ResourceSet;
};

class Device
{
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
    virtual IBusOperations& GetBusDeviceOperations();

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
    dma::Tag* d_DMA_tag = nullptr;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
};

// Create device function, used while probing
typedef Device* (*CreateDeviceFunc)(const CreateDeviceProperties& cdp);

namespace device_manager
{
    using DeviceList = ::device::DeviceList;

    int AllocateMajor();
    Result AttachSingle(Device& device);
    Result Detach(Device& device);
    Device* AttachChild(Device& bus, const ResourceSet& resourceSet);
    void AttachBus(Device& bus);
    Device* FindDevice(const char* name);
    Device* FindDevice(dev_t dev);
    Device* CreateDevice(const char* driver, const CreateDeviceProperties& cdp);

} // namespace device_manager
