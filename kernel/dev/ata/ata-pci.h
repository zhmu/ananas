/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_ATA_PCI_H
#define ANANAS_ATA_PCI_H

#include "kernel/device.h"
#include "ata.h"

namespace ata
{
    class ATAPCI : public Device, private IDeviceOperations
    {
      public:
        using Device::Device;
        virtual ~ATAPCI() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;
    };

} // namespace ata

#endif // ANANAS_ATA_PCI_H
