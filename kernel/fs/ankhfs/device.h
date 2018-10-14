#ifndef ANANAS_ANKFS_DEVICE_H
#define ANANAS_ANKFS_DEVICE_H

#include <ananas/types.h>

namespace ankhfs {
namespace devices {
constexpr int subRoot = 0;
constexpr int subDevices = 1;
constexpr int subDrivers = 2;
} // namespace devices

class IAnkhSubSystem;

IAnkhSubSystem& GetDeviceSubSystem();

} // namespace ankhfs

#endif // ANANAS_ANKFS_DEVICE_H
