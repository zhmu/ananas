/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef ANANAS_VCONSOLE_H
#define ANANAS_VCONSOLE_H

#include <ananas/types.h>
#include <ananas/util/array.h>
#include "kernel/device.h"
#include "kernel/dev/inputmux.h"

class IVideo;
class VTTY;

struct VConsole : public Device,
                  private IDeviceOperations,
                  private ICharDeviceOperations,
                  private input_mux::IInputConsumer {
    using Device::Device;
    virtual ~VConsole() = default;

    IDeviceOperations& GetDeviceOperations() override { return *this; }

    ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

    IVideo& GetVideo() { return *v_Video; }

    Result Attach() override;
    Result Detach() override;

    Result Read(VFS_FILE& file, void* buf, size_t len) override;
    Result Write(VFS_FILE& file, const void* buf, size_t len) override;

    void OnEvent(const AIMX_EVENT&) override;

  private:
    constexpr static size_t NumberOfVTTYs = 4;
    util::array<VTTY*, NumberOfVTTYs> vttys;

    IVideo* v_Video = nullptr;
    VTTY* activeVTTY = nullptr;
};

#endif /* ANANAS_VCONSOLE_H */
