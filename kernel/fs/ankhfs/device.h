/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

namespace ankhfs
{
    namespace devices
    {
        constexpr inline int subRoot = 0;
        constexpr inline int subDevices = 1;
        constexpr inline int subDrivers = 2;
    } // namespace devices

    class IAnkhSubSystem;

    IAnkhSubSystem& GetDeviceSubSystem();

} // namespace ankhfs
