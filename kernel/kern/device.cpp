/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/console.h"
#include "kernel/device.h"
#include "kernel/lib.h"

namespace device_manager::internal
{
    void OnDeviceDestruction(Device&);
} // namespace device_manager::internal

Device::Device() : d_Parent(this) {}

Device::Device(const CreateDeviceProperties& cdp)
    : d_Parent(cdp.cdp_Parent), d_ResourceSet(cdp.cdp_ResourceSet)
{
}

Device::~Device() { device_manager::internal::OnDeviceDestruction(*this); }

void Device::Printf(const char* fmt, ...) const
{
#define DEVICE_PRINTF_BUFSIZE 256
    char buf[DEVICE_PRINTF_BUFSIZE];

    snprintf(buf, sizeof(buf), "%s: ", d_Name);

    va_list va;
    va_start(va, fmt);
    vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 2, fmt, va);
    buf[sizeof(buf) - 2] = '\0';
    strcat(buf, "\n");
    va_end(va);

    console_putstring(buf);
}

IBusOperations& Device::GetBusDeviceOperations()
{
    KASSERT(d_Parent != nullptr, "no parent bus for device %s", d_Name);
    return d_Parent->GetBusDeviceOperations();
}
