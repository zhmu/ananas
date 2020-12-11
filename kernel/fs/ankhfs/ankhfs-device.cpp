/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "device.h"
#include "support.h"

namespace device_manager::internal
{
    extern Spinlock spl_devicequeue;
    extern device::DeviceList deviceList;
} // namespace device_manager::internal

namespace driver_manager::internal
{
    DriverList& GetDriverList();
} // namespace driver_manager::internal

namespace ankhfs
{
    namespace
    {
        Result HandleReadDir_Device(struct VFS_FILE* file, void* dirents, size_t len)
        {
            struct FetchEntry : IReadDirCallback {
                bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) override
                {
                    // XXX we should lock the device here

                    // Skip anything that isn't a char/block device - we can do nothing with those
                    while (currentDevice != device_manager::internal::deviceList.end() &&
                           currentDevice->GetCharDeviceOperations() == nullptr &&
                           currentDevice->GetBIODeviceOperations() == nullptr)
                        ++currentDevice;
                    if (currentDevice == device_manager::internal::deviceList.end())
                        return false;

                    snprintf(
                        entry, maxLength, "%s", currentDevice->d_Name);
                    inum = make_inum(
                        SS_Device, makedev(currentDevice->d_Major, currentDevice->d_Unit), 0);
                    ++currentDevice;
                    return true;
                }

                device::DeviceList::iterator currentDevice =
                    device_manager::internal::deviceList.begin();
            };

            // Fill the root directory with one entry per device
            SpinlockGuard g(device_manager::internal::spl_devicequeue);
            FetchEntry entryFetcher;
            return HandleReadDir(file, dirents, len, entryFetcher);
        }

        class DeviceSubSystem : public IAnkhSubSystem
        {
          public:
            Result HandleReadDir(struct VFS_FILE* file, void* dirents, size_t len) override
            {
                return HandleReadDir_Device(file, dirents, len);
            }

            Result FillInode(INode& inode, ino_t inum) override
            {
                // Special-case the non-device entries; we can't look them up
                if (inum_to_id(inum) == 0) {
                    switch (inum_to_sub(inum)) {
                        case devices::subRoot:
                            inode.i_sb.st_mode |= S_IFDIR;
                            return Result::Success();
                        case devices::subDevices:
                        case devices::subDrivers:
                            inode.i_sb.st_mode |= S_IFREG;
                            return Result::Success();
                    }
                }

                dev_t devno = static_cast<dev_t>(inum_to_id(inum));
                Device* device = device_manager::FindDevice(devno);
                if (device == nullptr)
                    return RESULT_MAKE_FAILURE(EIO);

                if (device->GetCharDeviceOperations() != nullptr) {
                    inode.i_sb.st_mode |= S_IFCHR;
                } else if (device->GetBIODeviceOperations() != nullptr) {
                    inode.i_sb.st_mode |= S_IFBLK;
                }
                return Result::Success();
            }

            Result HandleRead(struct VFS_FILE* file, void* buf, size_t len) override
            {
                ino_t inum = file->f_dentry->d_inode->i_inum;

                dev_t devno = static_cast<dev_t>(inum_to_id(inum));
                if (devno == 0) {
                    char result[1024]; // XXX
                    strcpy(result, "???");
                    switch (inum_to_sub(inum)) {
                        case devices::subDevices: {
                            SpinlockGuard g(device_manager::internal::spl_devicequeue);

                            char* r = result;
                            for (auto& device : device_manager::internal::deviceList) {
                                snprintf(
                                    r, sizeof(result) - (r - result), "%s %d %d %c%c%c%c%c\n",
                                    device.d_Name, device.d_Major, device.d_Unit,
                                    device.GetBIODeviceOperations() != nullptr ? 'b' : '.',
                                    device.GetCharDeviceOperations() != nullptr ? 'c' : '.',
                                    device.GetUSBHubDeviceOperations() != nullptr ? 'h' : '.',
                                    device.GetSCSIDeviceOperations() != nullptr ? 's' : '.',
                                    device.GetUSBDeviceOperations() != nullptr ? 'u' : '.');
                                r += strlen(r);
                            }
                            break;
                        }
                        case devices::subDrivers: {
                            char* r = result;
                            auto& driverList = driver_manager::internal::GetDriverList();
                            for (auto& d : driverList) {
                                snprintf(
                                    r, sizeof(result) - (r - result), "%s %d %d %d\n", d.d_Name,
                                    d.d_Priority, d.d_Major, d.d_CurrentUnit);
                                r += strlen(r);
                            }
                            break;
                        }
                    }
                    return ankhfs::HandleRead(file, buf, len, result);
                }

                return RESULT_MAKE_FAILURE(EIO);
            }

            Result HandleOpen(VFS_FILE& file, Process* p) override
            {
                auto inum = file.f_dentry->d_inode->i_inum;
                dev_t devno = static_cast<dev_t>(inum_to_id(inum));
                Device* device = device_manager::FindDevice(devno);
                if (device == nullptr)
                    return RESULT_MAKE_FAILURE(EIO);

                if (auto result = device->GetDeviceOperations().Open(p); result.IsFailure())
                    return result;

                file.f_device = device; // XXX we should refcount this
                return Result::Success();
            }

            Result HandleClose(VFS_FILE& file, Process* p) override
            {
                auto inum = file.f_dentry->d_inode->i_inum;
                dev_t devno = static_cast<dev_t>(inum_to_id(inum));
                Device* device = device_manager::FindDevice(devno);
                if (device != nullptr) {
                    if (auto result = device->GetDeviceOperations().Close(p); result.IsFailure())
                        return result;
                }
                return Result::Success();
            }

            Result HandleIOControl(struct VFS_FILE* file, unsigned long op, void* args[]) override
            {
                return RESULT_MAKE_FAILURE(EIO);
            }

            Result HandleReadLink(INode& inode, void* buf, size_t len) override
            {
                return RESULT_MAKE_FAILURE(EIO);
            }
        };

    } // unnamed namespace

    IAnkhSubSystem& GetDeviceSubSystem()
    {
        static DeviceSubSystem deviceSubSystem;
        return deviceSubSystem;
    }

} // namespace ankhfs
