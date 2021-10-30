/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/util/array.h>
#include <ananas/inputmux.h>
#include "kernel/dev/inputmux.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "../core/config.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"

namespace
{
    // Modifier byte is defined in HID 1.11 8.3
    namespace modifierByte
    {
        constexpr uint8_t LeftControl = (1 << 0);
        constexpr uint8_t LeftShift = (1 << 1);
        constexpr uint8_t LeftAlt = (1 << 2);
        constexpr uint8_t LeftGui = (1 << 3);
        constexpr uint8_t RightControl = (1 << 4);
        constexpr uint8_t RightShift = (1 << 5);
        constexpr uint8_t RightAlt = (1 << 6);
        constexpr uint8_t RightGui = (1 << 7);
    } // namespace modifierByte

    struct KeyMap {
        AIMX_KEY_CODE standard;
        AIMX_KEY_CODE shift;
    };

    // As outlined in USB HID Usage Tables 1.12, chapter 10
    constexpr util::array<KeyMap, 128> keymap{
        /* 00 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 01 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 02 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 03 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 04 */ KeyMap{AIMX_KEY_A,             AIMX_KEY_A},
        /* 05 */ KeyMap{AIMX_KEY_B,             AIMX_KEY_B},
        /* 06 */ KeyMap{AIMX_KEY_C,             AIMX_KEY_C},
        /* 07 */ KeyMap{AIMX_KEY_D,             AIMX_KEY_D},
        /* 08 */ KeyMap{AIMX_KEY_E,             AIMX_KEY_E},
        /* 09 */ KeyMap{AIMX_KEY_F,             AIMX_KEY_F},
        /* 0a */ KeyMap{AIMX_KEY_G,             AIMX_KEY_G},
        /* 0b */ KeyMap{AIMX_KEY_H,             AIMX_KEY_H},
        /* 0c */ KeyMap{AIMX_KEY_I,             AIMX_KEY_I},
        /* 0d */ KeyMap{AIMX_KEY_J,             AIMX_KEY_J},
        /* 0e */ KeyMap{AIMX_KEY_K,             AIMX_KEY_K},
        /* 0f */ KeyMap{AIMX_KEY_L,             AIMX_KEY_L},
        /* 10 */ KeyMap{AIMX_KEY_M,             AIMX_KEY_M},
        /* 11 */ KeyMap{AIMX_KEY_N,             AIMX_KEY_N},
        /* 12 */ KeyMap{AIMX_KEY_O,             AIMX_KEY_O},
        /* 13 */ KeyMap{AIMX_KEY_P,             AIMX_KEY_P},
        /* 14 */ KeyMap{AIMX_KEY_Q,             AIMX_KEY_Q},
        /* 15 */ KeyMap{AIMX_KEY_R,             AIMX_KEY_R},
        /* 16 */ KeyMap{AIMX_KEY_S,             AIMX_KEY_S},
        /* 17 */ KeyMap{AIMX_KEY_T,             AIMX_KEY_T},
        /* 18 */ KeyMap{AIMX_KEY_U,             AIMX_KEY_U},
        /* 19 */ KeyMap{AIMX_KEY_V,             AIMX_KEY_V},
        /* 1a */ KeyMap{AIMX_KEY_W,             AIMX_KEY_W},
        /* 1b */ KeyMap{AIMX_KEY_X,             AIMX_KEY_X},
        /* 1c */ KeyMap{AIMX_KEY_Y,             AIMX_KEY_Y},
        /* 1d */ KeyMap{AIMX_KEY_Z,             AIMX_KEY_Z},
        /* 1e */ KeyMap{'1',                    '!'},
        /* 1f */ KeyMap{'2',                    '@'},
        /* 20 */ KeyMap{'3',                    '#'},
        /* 21 */ KeyMap{'4',                    '$'},
        /* 22 */ KeyMap{'5',                    '%'},
        /* 23 */ KeyMap{'6',                    '^'},
        /* 24 */ KeyMap{'7',                    '&'},
        /* 25 */ KeyMap{'8',                    '*'},
        /* 26 */ KeyMap{'9',                    '('},
        /* 27 */ KeyMap{'0',                    ')'},
        /* 28 */ KeyMap{0x0d,                   0x0d},
        /* 29 */ KeyMap{0x1b,                   0x1b},
        /* 2a */ KeyMap{0x08,                   0x08},
        /* 2b */ KeyMap{0x09,                   0x09},
        /* 2c */ KeyMap{' ',                    ' '},
        /* 2d */ KeyMap{'-',                    '_'},
        /* 2e */ KeyMap{'=',                    '+'},
        /* 2f */ KeyMap{'[',                    '{'},
        /* 30 */ KeyMap{']',                    '}'},
        /* 31 */ KeyMap{'\\',                   '|'},
        /* 32 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 33 */ KeyMap{';',                    ':'},
        /* 34 */ KeyMap{'\'',                   '"'},
        /* 35 */ KeyMap{'`',                    '~'},
        /* 36 */ KeyMap{',',                    '<'},
        /* 37 */ KeyMap{'.',                    '>'},
        /* 38 */ KeyMap{'/',                    '?'},
        /* 39 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 3a */ KeyMap{AIMX_KEY_F1,            AIMX_KEY_F1},
        /* 3b */ KeyMap{AIMX_KEY_F2,            AIMX_KEY_F2},
        /* 3c */ KeyMap{AIMX_KEY_F3,            AIMX_KEY_F3},
        /* 3d */ KeyMap{AIMX_KEY_F4,            AIMX_KEY_F4},
        /* 3e */ KeyMap{AIMX_KEY_F5,            AIMX_KEY_F5},
        /* 3f */ KeyMap{AIMX_KEY_F6,            AIMX_KEY_F6},
        /* 40 */ KeyMap{AIMX_KEY_F7,            AIMX_KEY_F7},
        /* 41 */ KeyMap{AIMX_KEY_F8,            AIMX_KEY_F8},
        /* 42 */ KeyMap{AIMX_KEY_F9,            AIMX_KEY_F9},
        /* 43 */ KeyMap{AIMX_KEY_F10,           AIMX_KEY_F10},
        /* 44 */ KeyMap{AIMX_KEY_F11,           AIMX_KEY_F11},
        /* 45 */ KeyMap{AIMX_KEY_F12,           AIMX_KEY_F12},
        /* 46 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 47 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 48 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 49 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 4a */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 4b */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 4c */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 4d */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 4e */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 4f */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 50 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 51 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 52 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 53 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 54 */ KeyMap{'/',                    '/'},
        /* 55 */ KeyMap{'*',                    '*'},
        /* 56 */ KeyMap{'-',                    '-'},
        /* 57 */ KeyMap{'+',                    '+'},
        /* 58 */ KeyMap{0x0d,                   0x0d},
        /* 59 */ KeyMap{'1',                    '1'},
        /* 5a */ KeyMap{'2',                    '2'},
        /* 5b */ KeyMap{'3',                    '3'},
        /* 5c */ KeyMap{'4',                    '4'},
        /* 5d */ KeyMap{'5',                    '5'},
        /* 5e */ KeyMap{'6',                    '6'},
        /* 5f */ KeyMap{'7',                    '7'},
        /* 60 */ KeyMap{'8',                    '8'},
        /* 61 */ KeyMap{'9',                    '9'},
        /* 62 */ KeyMap{'0',                    '0'},
        /* 63 */ KeyMap{'.',                    '.'},
        /* 64 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 65 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 66 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 67 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 68 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 69 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 6a */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 6b */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 6c */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 6d */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 6e */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 6f */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 70 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 71 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 72 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 73 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 74 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 75 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 76 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 77 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 78 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 79 */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 7a */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 7b */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 7c */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 7d */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 7e */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
        /* 7f */ KeyMap{AIMX_KEY_NONE,          AIMX_KEY_NONE},
    };

    class USBKeyboard : public Device, private IDeviceOperations, private usb::IPipeCallback
    {
      public:
        using Device::Device;
        virtual ~USBKeyboard() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

      protected:
        void OnPipeCallback(usb::Pipe& pipe) override;

      private:
        usb::USBDevice* uk_Device = nullptr;
        usb::Pipe* uk_Pipe = nullptr;
    };

    Result USBKeyboard::Attach()
    {
        uk_Device = static_cast<usb::USBDevice*>(
            d_ResourceSet.AllocateResource(Resource::RT_USB_Device, 0));

        if (auto result =
                uk_Device->AllocatePipe(0, TRANSFER_TYPE_INTERRUPT, EP_DIR_IN, 0, *this, uk_Pipe);
            result.IsFailure()) {
            Printf("endpoint 0 not interrupt/in");
            return result;
        }
        return uk_Pipe->Start();
    }

    Result USBKeyboard::Detach()
    {
        if (uk_Device == nullptr)
            return Result::Success();

        if (uk_Pipe != nullptr)
            uk_Device->FreePipe(*uk_Pipe);

        uk_Pipe = nullptr;
        return Result::Success();
    }

    void USBKeyboard::OnPipeCallback(usb::Pipe& pipe)
    {
        usb::Transfer& xfer = pipe.p_xfer;

        if (xfer.t_flags & TRANSFER_FLAG_ERROR)
            return;

        // See if there's anything worthwhile to report here. We lazily use the USB boot class as
        // it's much easier to process: HID 1.1 B.1 Protocol 1 (keyboard) lists everything
        for (int n = 2; n < 8; n++) {
            int scancode = xfer.t_data[n];
            if (scancode == 0 || scancode > keymap.size())
                continue;

            const int modifiers = [](int d) {
                int m = 0;
                if (d & modifierByte::LeftShift)
                    m |= AIMX_MOD_LEFT_SHIFT;
                if (d & modifierByte::RightShift)
                    m |= AIMX_MOD_RIGHT_SHIFT;
                if (d & modifierByte::LeftControl)
                    m |= AIMX_MOD_LEFT_CONTROL;
                if (d & modifierByte::LeftGui)
                    m |= AIMX_MOD_LEFT_GUI;
                if (d & modifierByte::RightShift)
                    m |= AIMX_MOD_RIGHT_SHIFT;
                if (d & modifierByte::RightShift)
                    m |= AIMX_MOD_RIGHT_SHIFT;
                if (d & modifierByte::RightControl)
                    m |= AIMX_MOD_RIGHT_CONTROL;
                if (d & modifierByte::RightGui)
                    m |= AIMX_MOD_LEFT_GUI;
                return m;
            }(xfer.t_data[0]);

            // Look up the scancode
            const auto& key = [](int scancode, int modifiers) {
                const auto& km = keymap[scancode];
                if (modifiers & (AIMX_MOD_LEFT_CONTROL | AIMX_MOD_RIGHT_CONTROL))
                    return km.standard;

                if (modifiers & (AIMX_MOD_LEFT_SHIFT | AIMX_MOD_RIGHT_SHIFT))
                    return km.shift;

                return km.standard;
            }(scancode, modifiers);

            if (key != AIMX_KEY_NONE) {
                // TODO Verify how to get specific down/up events here
                input_mux::OnEvent({ AIMX_EVENT_KEY_DOWN, key });
                input_mux::OnEvent({ AIMX_EVENT_KEY_UP, key });
            }
        }

        /* Reschedule the pipe for future updates */
        uk_Pipe->Start();
    }

    struct USBKeyboard_Driver : public Driver {
        USBKeyboard_Driver() : Driver("usbkeyboard") {}

        const char* GetBussesToProbeOn() const override { return "usbbus"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_USB_Device, 0);
            if (res == nullptr)
                return nullptr;

            auto usb_dev = static_cast<usb::USBDevice*>(reinterpret_cast<void*>(res->r_Base));

            auto& iface = usb_dev->ud_interface[usb_dev->ud_cur_interface];
            if (iface.if_class == USB_IF_CLASS_HID && iface.if_subclass == 1 /* boot interface */ &&
                iface.if_protocol == 1 /* keyboard */)
                return new USBKeyboard(cdp);
            return nullptr;
        }
    };

    const RegisterDriver<USBKeyboard_Driver> registerDriver;

} // unnamed namespace
