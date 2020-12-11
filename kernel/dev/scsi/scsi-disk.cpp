/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/endian.h"
#include "kernel/lib.h"
#include "kernel/mbr.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "scsi.h"

using Direction = ISCSIDeviceOperations::Direction;

namespace
{
    void copy_string(char* dest, int dest_len, const uint8_t* src, int src_len)
    {
        /* First step is to copy with \0-termination */
        memcpy(dest, src, src_len > dest_len ? dest_len : src_len);
        dest[dest_len] = '\0';

        /* Remove any trailing whitespace and \0-terminate */
        int n = dest_len - 1;
        while (n > 0 && (dest[n] == '\0' || dest[n] == ' '))
            n--;
        dest[n] = '\0';
    }

    void DumpSenseData(const struct SCSI_FIXED_SENSE_DATA& sd)
    {
        kprintf(
            "sense data: code %d (%c) flags %c%c%c key %d\n",
            SCSI_FIXED_SENSE_CODE_CODE(sd.sd_code),
            (sd.sd_code & SCSI_FIXED_SENSE_CODE_VALID) ? 'V' : '.',
            (sd.sd_flags & SCSI_FIXED_SENSE_FLAGS_FILEMARK) ? 'F' : '.',
            (sd.sd_flags & SCSI_FIXED_SENSE_FLAGS_EOM) ? 'E' : '.',
            (sd.sd_flags & SCSI_FIXED_SENSE_FLAGS_ILI) ? 'I' : '.',
            SCSI_FIXED_SENSE_FLAGS_KEY(sd.sd_flags));
        kprintf(
            "info 0x%x 0x%x 0x%x 0x%x add length %d\n", sd.sd_info[0], sd.sd_info[1], sd.sd_info[2],
            sd.sd_info[3], sd.sd_add_length);
        kprintf(
            "command info 0x%x 0x%x 0x%x 0x%x\n", sd.sd_cmd_info[0], sd.sd_cmd_info[1],
            sd.sd_cmd_info[2], sd.sd_cmd_info[3]);
        kprintf(
            "add sense code %d qualifier %d field replace code %d\n", sd.sd_add_sense_code,
            sd.sd_add_sense_qualifier, sd.sd_field_replace_code);
    }

    class SCSIDisk : public Device, private IDeviceOperations, private IBIODeviceOperations
    {
      public:
        using Device::Device;
        virtual ~SCSIDisk() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        IBIODeviceOperations* GetBIODeviceOperations() override { return this; }

        Result HandleRequest(
            int lun, Direction dir, const void* cb, size_t cb_len, void* result,
            size_t* result_len);

        Result Attach() override;
        Result Detach() override;

        void ReadBIO(BIO& bio) override;
        void WriteBIO(BIO& bio) override;
    };

    Result SCSIDisk::HandleRequest(
        int lun, Direction dir, const void* cb, size_t cb_len, void* result, size_t* result_len)
    {
        return d_Parent->GetSCSIDeviceOperations()->PerformSCSIRequest(
            lun, dir, cb, cb_len, result, result_len);
    }

    Result SCSIDisk::Attach()
    {
        /* Do a SCSI INQUIRY command; we only use the vendor/product ID for now */
        struct SCSI_INQUIRY_6_REPLY inq_reply;
        struct SCSI_INQUIRY_6_CMD inq_cmd;
        memset(&inq_cmd, 0, sizeof inq_cmd);
        inq_cmd.c_code = SCSI_CMD_INQUIRY_6;

        size_t reply_len = sizeof(inq_reply);
        if (auto result = HandleRequest(
                0, Direction::D_In, &inq_cmd, sizeof(inq_cmd), &inq_reply, &reply_len);
            result.IsFailure())
            return result;

        /*
         * We expect the device to be unavailable (as it's just reset) - we'll issue
         * a 'TEST UNIT READY' and if that fails, we'll do a 'REQUEST SENSE' so the
         * device ought to reset itself.
         */
        struct SCSI_TEST_UNIT_READY_CMD tur_cmd;
        memset(&tur_cmd, 0, sizeof tur_cmd);
        tur_cmd.c_code = SCSI_CMD_TEST_UNIT_READY;
        if (auto result = HandleRequest(0, Direction::D_In, &tur_cmd, sizeof(tur_cmd), NULL, NULL);
            result.IsFailure()) {
            /* This is expected; issue 'REQUEST SENSE' to reset the status */
            struct SCSI_REQUEST_SENSE_CMD rs_cmd;
            struct SCSI_FIXED_SENSE_DATA sd;
            memset(&rs_cmd, 0, sizeof rs_cmd);
            rs_cmd.c_code = SCSI_CMD_REQUEST_SENSE;
            reply_len = sizeof(sd);
            if (auto result =
                    HandleRequest(0, Direction::D_In, &rs_cmd, sizeof(rs_cmd), &sd, &reply_len);
                result.IsFailure()) {
                Printf("handle_req: err=%d len %d", result.AsStatusCode(), reply_len);
                DumpSenseData(sd);
            }
        }

        /* Now fetch the capacity */
        struct SCSI_READ_CAPACITY_10_CMD cap_cmd;
        memset(&cap_cmd, 0, sizeof cap_cmd);
        cap_cmd.c_code = SCSI_CMD_READ_CAPACITY_10;
        struct SCSI_READ_CAPACITY_10_REPLY cap_reply;
        reply_len = sizeof(cap_reply);
        if (auto result = HandleRequest(
                0, Direction::D_In, &cap_cmd, sizeof(cap_cmd), &cap_reply, &reply_len);
            result.IsFailure())
            return result;
        uint32_t num_lba = betoh32(cap_reply.r_lba);
        uint32_t block_len = betoh32(cap_reply.r_block_length);

        /* Now print some nice information */
        char vid[9], pid[17];
        copy_string(vid, sizeof(vid), inq_reply.r_vendor_id, sizeof(inq_reply.r_vendor_id));
        copy_string(pid, sizeof(pid), inq_reply.r_product_id, sizeof(inq_reply.r_product_id));
        Printf(
            "vendor <%s> product <%s> size %d MB", vid, pid, num_lba / ((1024 * 1024) / block_len));

        BIO* bio;
        if (auto result = bread(this, 0, BIO_SECTOR_SIZE, bio); result.IsFailure())
            return result;

        mbr_process(this, bio);
        bio->Release();

        return Result::Success();
    }

    Result SCSIDisk::Detach() { return Result::Success(); }

    void SCSIDisk::ReadBIO(struct BIO& bio)
    {
        KASSERT(bio.b_length > 0, "invalid length");
        KASSERT(bio.b_length % 512 == 0, "invalid length"); /* XXX */

        // XXX we could schedule things here, but seeing that this is only for USB
        // there is no real benefit right now

        struct SCSI_READ_10_CMD r_cmd;
        memset(&r_cmd, 0, sizeof r_cmd);
        r_cmd.c_code = SCSI_CMD_READ_10;
        r_cmd.c_lba = htobe32(bio.b_ioblock);
        r_cmd.c_transfer_len = htobe16(bio.b_length / 512);
        size_t reply_len = bio.b_length;
        auto result =
            HandleRequest(0, Direction::D_In, &r_cmd, sizeof(r_cmd), bio.Data(), &reply_len);
        bio.Done(result);
    }

    void SCSIDisk::WriteBIO(struct BIO& bio)
    {
        // XXX Not yet implemented
        Printf("TODO: implement write support");
        bio.Done(RESULT_MAKE_FAILURE(EIO));
    }

    struct SCSIDisk_Driver : public Driver {
        SCSIDisk_Driver() : Driver("scsidisk") {}

        const char* GetBussesToProbeOn() const override
        {
            return nullptr; // instantiated by whoever needs it
        }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            return new SCSIDisk(cdp);
        }
    };

    const RegisterDriver<SCSIDisk_Driver> registerDriver;

} // unnamed namespace
