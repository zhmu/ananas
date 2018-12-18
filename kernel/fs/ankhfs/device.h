/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_ANKFS_DEVICE_H
#define ANANAS_ANKFS_DEVICE_H

#include <ananas/types.h>

namespace ankhfs
{
    namespace devices
    {
        constexpr int subRoot = 0;
        constexpr int subDevices = 1;
        constexpr int subDrivers = 2;
    } // namespace devices

    class IAnkhSubSystem;

    IAnkhSubSystem& GetDeviceSubSystem();

} // namespace ankhfs

#endif // ANANAS_ANKFS_DEVICE_H
