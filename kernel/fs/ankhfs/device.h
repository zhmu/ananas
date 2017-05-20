#ifndef ANANAS_ANKFS_DEVICE_H
#define ANANAS_ANKFS_DEVICE_H

#include <ananas/types.h>

namespace Ananas {
namespace AnkhFS {
namespace Devices {
constexpr int subRoot = 0;
constexpr int subDevices = 1;
constexpr int subDrivers = 2;
} // namespace Devices

class IAnkhSubSystem;

IAnkhSubSystem& GetDeviceSubSystem();

} // namespace AnkhFS
} // namespace Ananas

#endif // ANANAS_ANKFS_DEVICE_H