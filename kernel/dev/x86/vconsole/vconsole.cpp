/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/console-driver.h"
#include "kernel/dev/tty.h"
#include "kernel/lib.h"
#include "framebuffer.h"
#include "vconsole.h"
#include "vga.h"
#include "vtty.h"
#include <ananas/inputmux.h>

Result VConsole::Attach()
{
    v_Video = nullptr;
    if (Framebuffer::IsUsable()) {
        v_Video = new Framebuffer;
    } else {
        v_Video = new VGA; // XXX may not be available
    }
    KASSERT(v_Video != nullptr, "no video attached?");

    for (auto& vtty : vttys) {
        vtty = static_cast<VTTY*>(
            device_manager::CreateDevice("vtty", CreateDeviceProperties(*this, ResourceSet())));
        if (auto result = device_manager::AttachSingle(*vtty); result.IsFailure())
            panic("cannot attach vtty (%d)", result.AsStatusCode());
    }
    activeVTTY = vttys.front();
    activeVTTY->Activate();

    input_mux::RegisterConsumer(*this);
    return Result::Success();
}

Result VConsole::Detach()
{
    panic("TODO");

    for (auto& vtty : vttys) {
        if (auto result = vtty->Detach(); result.IsFailure())
            panic("cannot detach vtty (%d)", result.AsStatusCode());
    }
    input_mux::UnregisterConsumer(*this);
    delete v_Video;
    return Result::Success();
}

Result VConsole::Read(VFS_FILE& file, void* buf, size_t len)
{
    return activeVTTY->Read(file, buf, len);
}

Result VConsole::Write(VFS_FILE& file, const void* buf, size_t len)
{
    return activeVTTY->Write(file, buf, len);
}

void VConsole::OnEvent(const AIMX_EVENT& event)
{
    if (event.type != AIMX_EVENT_KEY_DOWN) return;
    auto key = event.u.key_down.key;
    auto modifiers = event.u.key_down.mods;

    if (modifiers & (AIMX_MOD_LEFT_CONTROL | AIMX_MOD_RIGHT_CONTROL)) {
        if (key >= 'a' && key <= 'z') {
            char ch = (key - 'a') + 1; // control-a => 1, etc
            activeVTTY->OnInput(&ch, 1);
        } else
            return; // swallow the key
    }

    if (key >= AIMX_KEY_F1 && key <= AIMX_KEY_F12) {
        const int desiredVTTY = key - AIMX_KEY_F1;
        if (desiredVTTY < vttys.size()) {
            activeVTTY->Deactivate();
            activeVTTY = vttys[desiredVTTY];
            activeVTTY->Activate();
            return;
        }
    }

    if (key > 0 && key < 128) {
        char ch = key;
        activeVTTY->OnInput(&ch, 1);
    }
}

namespace
{
    struct VConsole_Driver : public ConsoleDriver {
        VConsole_Driver() : ConsoleDriver("vconsole", 100) {}

        const char* GetBussesToProbeOn() const override { return "corebus"; }

        Device* ProbeDevice() override { return new VConsole(ResourceSet()); }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            return nullptr; // we expect to be probed
        }
    };

    const RegisterDriver<VConsole_Driver> registerDriver;

} // unnamed namespace
