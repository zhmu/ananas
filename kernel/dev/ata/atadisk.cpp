/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mbr.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel-md/io.h"
#include "ata.h"
#include "atadisk.h"
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

    Result ATADisk::Attach()
    {
        disk_unit = (int)(uintptr_t)d_ResourceSet.AllocateResource(Resource::RT_ChildNum, 0);

        /* Calculate the length of the disk */
        disk_size = ATA_GET_DWORD(disk_identify.lba_sectors);
        if (ATA_GET_WORD(disk_identify.features2) & ATA_FEAT2_LBA48) {
            disk_size = ATA_GET_QWORD(disk_identify.lba48_sectors);
        }

        Printf("<%s> - %u MB", disk_identify.model, disk_size / ((1024UL * 1024UL) / 512UL));

        /* XXX We should check the registers to see if DMA support is configured */
        if (true) {
            disk_flags |= ATADISK_FLAG_DMA;
            Printf("using DMA transfers");
        }

        /*
         * Read the first sector and pass it to the MBR code; this is crude
         * and does not really belong here.
         */
        struct BIO* bio = bio_read(this, 0, BIO_SECTOR_SIZE);
        if (BIO_IS_ERROR(bio))
            return RESULT_MAKE_FAILURE(EIO); /* XXX should get error from bio */

        mbr_process(this, bio);
        bio_free(bio);
        return Result::Success();
    }

    Result ATADisk::Detach()
    {
        panic("unimplemented");
        return Result::Success();
    }

    void ATADisk::ReadBIO(BIO& bio)
    {
        struct ATA_REQUEST_ITEM item;
        KASSERT(bio.length > 0, "invalid length");
        KASSERT(bio.length % 512 == 0, "invalid length"); /* XXX */

        /* XXX boundary check */
        item.unit = disk_unit;
        item.lba = bio.io_block;
        item.count = bio.length / 512;
        item.bio = &bio;
        item.command = ATA_CMD_READ_SECTORS;
        item.flags = ATA_ITEM_FLAG_READ;
        if (disk_flags & ATADISK_FLAG_DMA)
            item.flags |= ATA_ITEM_FLAG_DMA;
        EnqueueAndStart(d_Parent, item);
    }

    void ATADisk::WriteBIO(BIO& bio)
    {
        struct ATA_REQUEST_ITEM item;
        KASSERT(bio.length > 0, "invalid length");
        KASSERT(bio.length % 512 == 0, "invalid length"); /* XXX */

        /* XXX boundary check */
        item.unit = disk_unit;
        item.lba = bio.io_block;
        item.count = bio.length / 512;
        item.bio = &bio;
        item.command = ATA_CMD_WRITE_SECTORS;
        item.flags = ATA_ITEM_FLAG_WRITE;
#ifdef NOTYET
        if (disk_flags & ATADISK_FLAG_DMA)
            item.flags |= ATA_ITEM_FLAG_DMA;
#endif
        EnqueueAndStart(d_Parent, item);
    }

    namespace
    {
        struct ATADisk_Driver : public Driver {
            ATADisk_Driver() : Driver("atadisk") {}

            const char* GetBussesToProbeOn() const override
            {
                return nullptr; // instantiated by ata-pci
            }

            Device* CreateDevice(const CreateDeviceProperties& cdp) override
            {
                return new ATADisk(cdp);
            }
        };

    } // unnamed namespace

    REGISTER_DRIVER(ATADisk_Driver)

} // namespace ata
