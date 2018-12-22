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

namespace {

size_t GetDeviceNameLengthToMatch(const Device& device)
{
    constexpr auto isDigit = [](char ch) {
        return ch >= '0' && ch <= '9';
    };

    // Cut off the unit number, if any
    size_t n = strlen(device.d_Name);
    while (n > 0 && isDigit(device.d_Name[n - 1]))
        n--;
    return n;
}

} // unnamed namespace


bool Driver::MustProbeOnBus(const Device& bus) const
{
    const char* ptr = GetBussesToProbeOn();
    if (ptr == nullptr)
        return false;

    auto bus_name_len = GetDeviceNameLengthToMatch(bus);
    while (*ptr != '\0') {
        const char* next = strchr(ptr, ',');
        if (next == NULL)
            next = strchr(ptr, '\0');

        size_t len = next - ptr;
        if (bus_name_len == len && strncmp(ptr, bus.d_Name, len) == 0) {
            return true;
        }

        if (*next == '\0')
            break;

        ptr = next + 1;
    }
    return false;
}
