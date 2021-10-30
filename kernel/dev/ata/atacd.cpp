/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/bio.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mbr.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel-md/io.h"
#include "ata.h"
#include "atacd.h"
#include "ata-controller.h"

namespace ata
{
    namespace
    {
        void EnqueueAndStart(Device* parent, ATA_REQUEST_ITEM& item)
        {
            // XXX this is a hack
            auto atacl = static_cast<ata::ATAController*>(parent);
            atacl->Enqueue(&item);
            atacl->Start();
        }

    } // unnamed namespace

    Result ATACD::Attach()
    {
        ata_unit = (int)(uintptr_t)d_ResourceSet.AllocateResource(Resource::RT_ChildNum, 0);

        Printf("<%s>", ata_identify.model);

        return Result::Success();
    }

    Result ATACD::Detach()
    {
        panic("unimplemented");
        return Result::Success();
    }

    Result ATACD::ReadBIO(struct BIO& bio)
    {
        struct ATA_REQUEST_ITEM item;
        KASSERT(bio.length == 2048, "invalid length"); /* XXX */

        /* XXX boundary check */
        memset(&item, 0, sizeof(item));
        item.unit = ata_unit;
        item.count = bio.length;
        item.bio = &bio;
        item.command = ATA_CMD_PACKET;
        item.flags = ATA_ITEM_FLAG_READ | ATA_ITEM_FLAG_ATAPI;
        item.atapi_command[0] = ATAPI_CMD_READ_SECTORS;
        item.atapi_command[2] = (bio.io_block >> 24) & 0xff;
        item.atapi_command[3] = (bio.io_block >> 16) & 0xff;
        item.atapi_command[4] = (bio.io_block >> 8) & 0xff;
        item.atapi_command[5] = (bio.io_block) & 0xff;
        item.atapi_command[7] = (bio.length / 2048) >> 8;
        item.atapi_command[8] = (bio.length / 2048) & 0xff;
        EnqueueAndStart(d_Parent, item);

        return Result::Success();
    }

    Result ATACD::WriteBIO(struct BIO& bio) { return Result::Failure(EROFS); }

    struct ATACD_Driver : public Driver {
        ATACD_Driver() : Driver("atacd") {}

        const char* GetBussesToProbeOn() const override
        {
            return nullptr; // instantiated by ata-pci
        }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override { return new ATACD(cdp); }
    };

    REGISTER_DRIVER(ATACD_Driver)

} // namespace ata
