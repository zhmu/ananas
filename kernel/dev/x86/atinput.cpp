/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <ananas/util/array.h>
#include <ananas/inputmux.h>
#include "kernel/dev/inputmux.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel-md/io.h"
#include "kernel-md/md.h"

namespace
{
    namespace scancode
    {
        constexpr uint8_t Escape = 0x01;
        constexpr uint8_t Control = 0x1d; // right-control has 0xe0 prefix
        constexpr uint8_t Tilde = 0x29;
        constexpr uint8_t LeftShift = 0x2a;
        constexpr uint8_t RightShift = 0x36;
        constexpr uint8_t Alt = 0x38; // right-alt has 0xe0 prefix
        constexpr uint8_t Delete = 0x53;
        constexpr uint8_t ReleasedBit = 0x80;
    } // namespace scancode

    namespace port
    {
        constexpr int Output = 0x60; // Read Only
        constexpr int Input = 0x60; // Write Only
        constexpr int Command = 0x64; // Write Only
        constexpr int Status = 0x64; // Read Only
    } // namespace port

    namespace command
    {
        constexpr uint8_t ReadControllerCommandByte = 0x20;
        constexpr uint8_t WriteControllerCommandByte = 0x60;
        constexpr uint8_t EnableAuxiliaryDeviceInterface = 0xa8;
        constexpr uint8_t DisableKeyboardInterface = 0xad;
        constexpr uint8_t EnableKeyboardInterface = 0xae;
        constexpr uint8_t WriteToAuxDevice = 0xd4;
        constexpr uint8_t ResetAuxiliaryDevice = 0xff;
    }

    namespace status
    {
        constexpr uint8_t OutputBufferFull = (1 << 0);
        constexpr uint8_t InputBufferFull = (1 << 1);
        constexpr uint8_t AuxiliaryData = (1 << 5);
    }

    namespace mouse0
    {
        constexpr uint8_t LeftButton = (1 << 0);
        constexpr uint8_t RightButton = (1 << 1);
        constexpr uint8_t MiddleButton = (1 << 2);
        constexpr uint8_t MustBeSet = (1 << 3);
        constexpr uint8_t XNegated = (1 << 4);
        constexpr uint8_t YNegated = (1 << 5);
        constexpr uint8_t XOverflow = (1 << 6);
        constexpr uint8_t YOverflow = (1 << 7);
    }

    struct KeyMap {
        AIMX_KEY_CODE standard;
        AIMX_KEY_CODE shift;
    };

    constexpr util::array<KeyMap, 128> keymap = {
        /* 00 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 01 */ KeyMap{0x1b, 0x1b},
        /* 02 */ KeyMap{'1', '!'},
        /* 03 */ KeyMap{'2', '@'},
        /* 04 */ KeyMap{'3', '#'},
        /* 05 */ KeyMap{'4', '$'},
        /* 06 */ KeyMap{'5', '%'},
        /* 07 */ KeyMap{'6', '^'},
        /* 08 */ KeyMap{'7', '&'},
        /* 09 */ KeyMap{'8', '*'},
        /* 0a */ KeyMap{'9', '('},
        /* 0b */ KeyMap{'0', ')'},
        /* 0c */ KeyMap{'-', '_'},
        /* 0d */ KeyMap{'=', '+'},
        /* 0e */ KeyMap{0x08, AIMX_KEY_NONE},
        /* 0f */ KeyMap{0x09, AIMX_KEY_NONE},
        /* 10 */ KeyMap{'q', 'Q'},
        /* 11 */ KeyMap{'w', 'W'},
        /* 12 */ KeyMap{'e', 'E'},
        /* 13 */ KeyMap{'r', 'R'},
        /* 14 */ KeyMap{'t', 'T'},
        /* 15 */ KeyMap{'y', 'Y'},
        /* 16 */ KeyMap{'u', 'U'},
        /* 17 */ KeyMap{'i', 'I'},
        /* 18 */ KeyMap{'o', 'O'},
        /* 19 */ KeyMap{'p', 'P'},
        /* 1a */ KeyMap{'[', '{'},
        /* 1b */ KeyMap{']', '}'},
        /* 1c */ KeyMap{0x0d, AIMX_KEY_NONE},
        /* 1d */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 1e */ KeyMap{'a', 'A'},
        /* 1f */ KeyMap{'s', 'S'},
        /* 20 */ KeyMap{'d', 'D'},
        /* 21 */ KeyMap{'f', 'F'},
        /* 22 */ KeyMap{'g', 'G'},
        /* 23 */ KeyMap{'h', 'H'},
        /* 24 */ KeyMap{'j', 'J'},
        /* 25 */ KeyMap{'k', 'K'},
        /* 26 */ KeyMap{'l', 'L'},
        /* 27 */ KeyMap{';', ':'},
        /* 28 */ KeyMap{'\'', '"'},
        /* 29 */ KeyMap{'`', '~'},
        /* 2a */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 2b */ KeyMap{'\\', '|'},
        /* 2c */ KeyMap{'z', 'Z'},
        /* 2d */ KeyMap{'x', 'X'},
        /* 2e */ KeyMap{'c', 'C'},
        /* 2f */ KeyMap{'v', 'V'},
        /* 30 */ KeyMap{'b', 'B'},
        /* 31 */ KeyMap{'n', 'N'},
        /* 32 */ KeyMap{'m', 'M'},
        /* 33 */ KeyMap{',', '<'},
        /* 34 */ KeyMap{'.', '>'},
        /* 35 */ KeyMap{'/', '?'},
        /* 36 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 37 */ KeyMap{'*', '*'},
        /* 38 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 39 */ KeyMap{' ', ' '},
        /* 3a */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 3b */ KeyMap{AIMX_KEY_F1, AIMX_KEY_F1},
        /* 3c */ KeyMap{AIMX_KEY_F2, AIMX_KEY_F2},
        /* 3d */ KeyMap{AIMX_KEY_F3, AIMX_KEY_F3},
        /* 3e */ KeyMap{AIMX_KEY_F4, AIMX_KEY_F4},
        /* 3f */ KeyMap{AIMX_KEY_F5, AIMX_KEY_F5},
        /* 40 */ KeyMap{AIMX_KEY_F6, AIMX_KEY_F6},
        /* 41 */ KeyMap{AIMX_KEY_F7, AIMX_KEY_F7},
        /* 42 */ KeyMap{AIMX_KEY_F8, AIMX_KEY_F8},
        /* 43 */ KeyMap{AIMX_KEY_F9, AIMX_KEY_F9},
        /* 44 */ KeyMap{AIMX_KEY_F10, AIMX_KEY_F10},
        /* 45 */ KeyMap{0, 0},
        /* 46 */ KeyMap{0, 0},
        /* 47 */ KeyMap{'7', '7'},
        /* 48 */ KeyMap{'8', '8'},
        /* 49 */ KeyMap{'9', '9'},
        /* 4a */ KeyMap{'-', '-'},
        /* 4b */ KeyMap{'4', '4'},
        /* 4c */ KeyMap{'5', '5'},
        /* 4d */ KeyMap{'6', '6'},
        /* 4e */ KeyMap{'+', '+'},
        /* 4f */ KeyMap{'1', '1'},
        /* 50 */ KeyMap{'2', '2'},
        /* 51 */ KeyMap{'3', '3'},
        /* 52 */ KeyMap{'0', '0'},
        /* 53 */ KeyMap{'.', '.'},
        /* 54 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 55 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 56 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 57 */ KeyMap{AIMX_KEY_F11, AIMX_KEY_F11},
        /* 58 */ KeyMap{AIMX_KEY_F12, AIMX_KEY_F12},
        /* 59 */ KeyMap{AIMX_KEY_NONE, ' '},
        /* 5a */ KeyMap{AIMX_KEY_NONE, ' '},
        /* 5b */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 5c */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 5d */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 5e */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 5f */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 60 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 61 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 62 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 63 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 64 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 65 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 66 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 67 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 68 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 69 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 6a */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 6b */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 6c */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 6d */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 6e */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 6f */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 70 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 71 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 72 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 73 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 74 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 75 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 76 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 77 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 78 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 79 */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 7a */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 7b */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 7c */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 7d */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 7e */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
        /* 7f */ KeyMap{AIMX_KEY_NONE, AIMX_KEY_NONE},
    };

    void WaitForReadFromOutput()
    {
        for(int n = 1'000'000; n > 0; --n) {
            if ((inb(port::Status) & status::OutputBufferFull) != 0)
                return;
        }
        kprintf("wait timeout inputBuffer 0\n");
    }

    void WaitForWriteToInput()
    {
        // Data should be written to the controller input buffer only if
        // the input-buffer-full bit (1) in the controller status register
        // (64) = 0
        for(int n = 1'000'000; n > 0; --n) {
            if ((inb(port::Status) & status::InputBufferFull) == 0)
                return;
        }
        kprintf("wait timeout inputBuffer 1\n");
    }

    void SendCommand(uint8_t cmd)
    {
        WaitForWriteToInput();
        outb(port::Command, cmd);
    }

    void WriteAux(uint8_t b)
    {
        SendCommand(command::WriteToAuxDevice);
        WaitForWriteToInput();
        outb(port::Input, b);
    }

    uint8_t ReadAux()
    {
        WaitForReadFromOutput();
        return inb(port::Output);
    }

    bool ProcessSpecialKeys(int scancode, int kbd_modifiers)
    {
        // Control-Shift-Escape causes us to drop in KDB
        if (kbd_modifiers == (AIMX_MOD_LEFT_CONTROL | AIMX_MOD_LEFT_SHIFT) &&
            scancode == scancode::Escape) {
            kdb::Enter("keyboard sequence");
            return true;
        }

        // Control-` / Control-~ triggers a panic
        if (kbd_modifiers == AIMX_MOD_LEFT_CONTROL && scancode == scancode::Tilde) {
            panic("forced by keyboard");
            return true;
        }

        if (kbd_modifiers == (AIMX_MOD_LEFT_CONTROL | AIMX_MOD_LEFT_ALT) &&
            scancode == scancode::Delete) {
            md::Reboot();
            return true;
        }

        return false;
    }

    struct ATKeyboard : public Device, private IDeviceOperations, private irq::IHandler
    {
      public:
        using Device::Device;
        virtual ~ATKeyboard() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

      private:
        irq::IRQResult OnIRQ() override;
        void OnKeyboardIRQ(uint8_t scancode);
        void OnAuxIRQ(uint8_t data);

      private:
        Spinlock io_lock;
        int kbd_modifiers = 0;
        int aux_state = 0;
        util::array<uint8_t, 4> aux_byte;
    };

    void ATKeyboard::OnKeyboardIRQ(uint8_t scancode)
    {
        const auto isReleased = (scancode & scancode::ReleasedBit) != 0;
        scancode &= ~scancode::ReleasedBit;

        // Handle setting control, alt, shift flags
        {
            auto setOrClearModifier = [&](int flag) {
                if (isReleased)
                    kbd_modifiers &= ~flag;
                else
                    kbd_modifiers |= flag;
            };

            switch (scancode) {
                case scancode::LeftShift:
                    setOrClearModifier(AIMX_MOD_LEFT_SHIFT);
                    return;
                case scancode::RightShift:
                    setOrClearModifier(AIMX_MOD_RIGHT_SHIFT);
                    return;
                case scancode::Alt:
                    setOrClearModifier(AIMX_MOD_LEFT_ALT);
                    return;
                case scancode::Control:
                    setOrClearModifier(AIMX_MOD_LEFT_CONTROL);
                    return;
            }
        }

        if (!isReleased && ProcessSpecialKeys(scancode, kbd_modifiers))
            return;

        // Look up the scancode
        const auto& key = [](int scancode, int modifiers) {
            const auto& km = keymap[scancode];
            if (modifiers & (AIMX_MOD_LEFT_CONTROL | AIMX_MOD_RIGHT_CONTROL))
                return km.standard;

            if (modifiers & (AIMX_MOD_LEFT_SHIFT | AIMX_MOD_RIGHT_SHIFT))
                return km.shift;

            return km.standard;
        }(scancode, kbd_modifiers);

        if (key != AIMX_KEY_NONE) {
            input_mux::OnEvent({ isReleased ? AIMX_EVENT_KEY_UP : AIMX_EVENT_KEY_DOWN, key, kbd_modifiers });
        }
    }

    void ATKeyboard::OnAuxIRQ(uint8_t data)
    {
        aux_byte[aux_state] = data;
        if (++aux_state == 3) {
            if (aux_byte[0] & mouse0::MustBeSet) {
                auto x = static_cast<int>(aux_byte[1]);
                auto y = static_cast<int>(aux_byte[2]);
                if (aux_byte[0] & mouse0::XNegated) x = x - 256;
                if (aux_byte[0] & mouse0::YNegated) y = y - 256;
                if (aux_byte[0] & mouse0::XOverflow) x = 0;
                if (aux_byte[0] & mouse0::YOverflow) y = 0;
                int button = 0;
                if (aux_byte[0] & mouse0::LeftButton) button |= AIMX_BUTTON_LEFT;
                if (aux_byte[0] & mouse0::RightButton) button |= AIMX_BUTTON_RIGHT;
                if (aux_byte[0] & mouse0::MiddleButton) button |= AIMX_BUTTON_MIDDLE;

                AIMX_EVENT event{ AIMX_EVENT_MOUSE };
                event.u.mouse = { button, x, -y };
                input_mux::OnEvent(event);
            }
            aux_state = 0;
        }
    }

    irq::IRQResult ATKeyboard::OnIRQ()
    {
        while (true) {
            const auto lockState = io_lock.LockUnpremptible();
            const auto status = inb(port::Status);
            if ((status & status::OutputBufferFull) == 0) {
                io_lock.UnlockUnpremptible(lockState);
                break;
            }

            const auto data = inb(port::Output);
            io_lock.UnlockUnpremptible(lockState);
            if (status & status::AuxiliaryData)
                OnAuxIRQ(data);
            else
                OnKeyboardIRQ(data);
        }

        return irq::IRQResult::Processed;
    }

    Result ATKeyboard::Attach()
    {
        auto res_io = d_ResourceSet.AllocateResource(Resource::RT_IO, 7);
        if (res_io == nullptr)
            return Result::Failure(ENODEV);

        if (reinterpret_cast<uintptr_t>(res_io) != port::Input) {
            Printf("I/O base %p does not match, aborting", res_io);
            return Result::Failure(ENXIO);
        }

        // Grab IRQ1 for PS/2 keyboard interrupts
        if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
            return result;
        // Grab IRQ12 for the PS/2 mouse interrupts
        if (auto result = irq::Register(12, this, irq::type::Default, *this); result.IsFailure())
            return result;

        SpinlockGuard g(io_lock);
        SendCommand(command::DisableKeyboardInterface);
        SendCommand(command::EnableAuxiliaryDeviceInterface);
        SendCommand(command::ResetAuxiliaryDevice);
        SendCommand(command::ReadControllerCommandByte);

        auto status = ReadAux();
        status |= 2; // enable aux interrupt
        SendCommand(command::WriteControllerCommandByte);
        WaitForWriteToInput();
        outb(port::Input, status);

        // Use default settings
        WriteAux(0xf6);
        ReadAux();

        // Enable mouse
        WriteAux(0xf4);
        ReadAux();

        SendCommand(command::EnableKeyboardInterface);

        // Drain output buffer
        while (inb(port::Status) & status::OutputBufferFull)
            inb(port::Output);
        return Result::Success();
    }

    Result ATKeyboard::Detach()
    {
        panic("detach");
        return Result::Success();
    }

    struct ATKeyboard_Driver : public Driver {
        ATKeyboard_Driver() : Driver("atinput") {}

        const char* GetBussesToProbeOn() const override { return "acpi"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PNP_ID, 0);
            if (res != NULL &&
                res->r_Base == 0x0303) /* PNP0303: IBM Enhanced (101/102-key, PS/2 mouse support) */
                return new ATKeyboard(cdp);
            return nullptr;
        }
    };

    const RegisterDriver<ATKeyboard_Driver> registerDriver;

} // unnamed namespace
