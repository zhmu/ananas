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
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/mm.h"

namespace
{
    class Slice : public Device, private IDeviceOperations, private IBIODeviceOperations
    {
      public:
        Slice(const CreateDeviceProperties& cdp) : Device(cdp) {}

        void SetFirstBlock(blocknr_t firstBlock) { slice_first_block = firstBlock; }

        void SetLength(blocknr_t length) { slice_length = length; }

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        IBIODeviceOperations* GetBIODeviceOperations() override { return this; }

        Result Attach() override { return Result::Success(); }

        Result Detach() override { return Result::Success(); }

        void ReadBIO(BIO& bio) override;
        void WriteBIO(BIO& bio) override;

      private:
        blocknr_t slice_first_block = 0;
        blocknr_t slice_length = 0;
    };

    void Slice::ReadBIO(BIO& bio)
    {
        bio.b_ioblock = bio.b_block + slice_first_block;
        d_Parent->GetBIODeviceOperations()->ReadBIO(bio);
    }

    void Slice::WriteBIO(BIO& bio)
    {
        bio.b_ioblock = bio.b_block + slice_first_block;
        d_Parent->GetBIODeviceOperations()->WriteBIO(bio);
    }

    struct Slice_Driver : public Driver {
        Slice_Driver() : Driver("slice") {}

        const char* GetBussesToProbeOn() const override
        {
            return nullptr; // instantiated by disk_mbr
        }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override { return new Slice(cdp); }
    };

    const RegisterDriver<Slice_Driver> registerDriver;

} // unnamed namespace

Device* slice_create(Device* parent, blocknr_t begin, blocknr_t length)
{
    auto slice = static_cast<Slice*>(
        device_manager::CreateDevice("slice", CreateDeviceProperties(*parent, ResourceSet())));
    if (slice != nullptr) {
        slice->SetFirstBlock(begin);
        slice->SetLength(length);
        if (auto result = device_manager::AttachSingle(*slice); result.IsFailure()) {
            delete slice;
            slice = nullptr;
        }
    }
    return slice;
}
