#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "device.h"
#include "support.h"

TRACE_SETUP;

namespace Ananas {
namespace DeviceManager {
namespace internal {
extern Spinlock spl_devicequeue;
extern device::DeviceList deviceList;
} // namespace internal
} // namespace DeviceManager

namespace DriverManager {
namespace internal {
Ananas::DriverList& GetDriverList();
} // namespace internal
} // namespace DriverManager

namespace AnkhFS {

namespace {

Result
HandleReadDir_Device(struct VFS_FILE* file, void* dirents, size_t* len)
{
	struct FetchEntry : IReadDirCallback {
		bool FetchNextEntry(char* entry, size_t maxLength, ino_t& inum) override {
			// XXX we should lock the device here

			// Skip anything that isn't a char/block device - we can do nothing with those
			while (currentDevice != DeviceManager::internal::deviceList.end() && currentDevice->GetCharDeviceOperations() == nullptr && currentDevice->GetBIODeviceOperations() == nullptr)
				++currentDevice;
			if (currentDevice == DeviceManager::internal::deviceList.end())
				return false;

			snprintf(entry, maxLength, "%s%d", currentDevice->d_Name, currentDevice->d_Unit);
			inum = make_inum(SS_Device, makedev(currentDevice->d_Major, currentDevice->d_Unit), 0);
			++currentDevice;
			return true;
		}

		device::DeviceList::iterator currentDevice = DeviceManager::internal::deviceList.begin();
	};

	// Fill the root directory with one entry per device
	SpinlockGuard g(DeviceManager::internal::spl_devicequeue);
	FetchEntry entryFetcher;
	return HandleReadDir(file, dirents, len, entryFetcher);
}

class DeviceSubSystem : public IAnkhSubSystem
{
public:
	Result HandleReadDir(struct VFS_FILE* file, void* dirents, size_t* len) override
	{
		return HandleReadDir_Device(file, dirents, len);
	}

	Result FillInode(INode& inode, ino_t inum) override
	{
		// Special-case the non-device entries; we can't look them up
		if (inum_to_id(inum) == 0) {
			switch(inum_to_sub(inum)) {
				case Devices::subRoot:
					inode.i_sb.st_mode |= S_IFDIR;
					return Result::Success();
				case Devices::subDevices:
				case Devices::subDrivers:
					inode.i_sb.st_mode |= S_IFREG;
					return Result::Success();
			}
		}

		dev_t devno = static_cast<dev_t>(inum_to_id(inum));
		Device* device = DeviceManager::FindDevice(devno);
		if (device == nullptr)
			return RESULT_MAKE_FAILURE(EIO);

		if (device->GetCharDeviceOperations() != nullptr) {
			inode.i_sb.st_mode |= S_IFCHR;
		} else if (device->GetBIODeviceOperations() != nullptr) {
			inode.i_sb.st_mode |= S_IFBLK;
		}
		return Result::Success();
	}

	Result HandleRead(struct VFS_FILE* file, void* buf, size_t* len) override
	{
		ino_t inum = file->f_dentry->d_inode->i_inum;

		dev_t devno = static_cast<dev_t>(inum_to_id(inum));
		if (devno == 0) {
			char result[1024]; // XXX
			strcpy(result, "???");
			switch(inum_to_sub(inum)) {
				case Devices::subDevices: {
					SpinlockGuard g(DeviceManager::internal::spl_devicequeue);

					char* r = result;
					for(auto& device: DeviceManager::internal::deviceList) {
						snprintf(r, sizeof(result) - (r - result), "%s %d %d %c%c%c%c%c\n",
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
				case Devices::subDrivers: {
					char* r = result;
					auto& driverList = Ananas::DriverManager::internal::GetDriverList();
					for(auto& d: driverList) {
						snprintf(r, sizeof(result) - (r - result), "%s %d %d %d\n",
						 d.d_Name, d.d_Priority, d.d_Major, d.d_CurrentUnit);
						r += strlen(r);
					}
					break;
				}
			}
			return AnkhFS::HandleRead(file, buf, len, result);
		}

		return RESULT_MAKE_FAILURE(EIO);
	}

	Result HandleOpen(VFS_FILE& file, Process* p) override
	{
		auto inum = file.f_dentry->d_inode->i_inum;
		dev_t devno = static_cast<dev_t>(inum_to_id(inum));
		Device* device = DeviceManager::FindDevice(devno);
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
		Device* device = DeviceManager::FindDevice(devno);
		if (device != nullptr) {
			if (auto result = device->GetDeviceOperations().Close(p); result.IsFailure())
				return result;
		}
		return Result::Success();
	}

	Result HandleIOControl(struct VFS_FILE* file, unsigned int op, void* args[]) override
	{
		return RESULT_MAKE_FAILURE(EIO);
	}

	Result HandleReadLink(INode& inode, void* buf, size_t* len) override
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

} // namespace AnkhFS
} // namespace Ananas

/* vim:set ts=2 sw=2: */
