/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
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
    namespace port
    {
        constexpr int Output = 0x60; // Read Only
        constexpr int Input = 0x60; // Write Only
        constexpr int Command = 0x64;
        constexpr int Status = 0x64;
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

    struct ATMouse : public Device, private IDeviceOperations, private irq::IHandler
    {
      public:
        using Device::Device;
        virtual ~ATMouse() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

      private:
        irq::IRQResult OnIRQ() override;

      private:
        int m_state{};
        util::array<uint8_t, 3> m_byte;
        int kbd_modifiers = 0;

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

        void Write(uint8_t b)
        {
            SendCommand(command::WriteToAuxDevice);
            WaitForWriteToInput();
            outb(port::Input, b);
        }

        uint8_t Read()
        {
            WaitForReadFromOutput();
            return inb(port::Output);
        }
    };

    irq::IRQResult ATMouse::OnIRQ()
    {
            auto st = inb(0x64);
        kprintf("ATMouse::OnIRQ, st = %x\n", st);
        //while(1) {
            auto status = inb(port::Status);
            kprintf("[st %x]", status);
            //if ((status & status::OutputBufferFull) == 0)
                //break;

            m_byte[m_state] = inb(port::Output);
            if (++m_state == 3) {
                if (m_byte[0] & mouse0::MustBeSet) {
                    auto x = static_cast<int>(m_byte[1]);
                    auto y = static_cast<int>(m_byte[2]);
                    if (m_byte[0] & mouse0::XNegated) x = -x;
                    if (m_byte[0] & mouse0::YNegated) y = -y;
                    int button = 0;
                    if (m_byte[0] & mouse0::LeftButton) button |= AIMX_BUTTON_LEFT;
                    if (m_byte[0] & mouse0::RightButton) button |= AIMX_BUTTON_RIGHT;
                    if (m_byte[0] & mouse0::MiddleButton) button |= AIMX_BUTTON_MIDDLE;

                    AIMX_EVENT event{ AIMX_EVENT_MOUSE };
                    event.u.mouse = { button, x, y };
                    input_mux::OnEvent(event);
                }
                m_state = 0;
            }
        //}
        kprintf("[/MSEIRQ]\n");
        return irq::IRQResult::Processed;
    }

    Result ATMouse::Attach()
    {
        // TODO We are sharing I/O with the PS/2 keyboard driver; we can't
        // handle this properly. The proper fix would be to have a PS/2 driver
        // with keyboard/mouse as children...
        if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
            return result;

        SendCommand(command::DisableKeyboardInterface);
        SendCommand(command::EnableAuxiliaryDeviceInterface);
        SendCommand(command::ResetAuxiliaryDevice);
        SendCommand(command::ReadControllerCommandByte);

        auto status = Read();
        status |= 2; // enable aux interrupt
        SendCommand(command::WriteControllerCommandByte);
        WaitForWriteToInput();
        outb(port::Input, status);

        // Use default settings
        Write(0xf6);
        Read();

        // Enable mouse
        Write(0xf4);
        Read();

        SendCommand(command::EnableKeyboardInterface);
        return Result::Success();
    }

    Result ATMouse::Detach()
    {
        panic("detach");
        return Result::Success();
    }

    struct ATMouse_Driver : public Driver {
        ATMouse_Driver() : Driver("atmouse") {}

        const char* GetBussesToProbeOn() const override { return "acpi"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PNP_ID, 0);
            if (res != NULL &&
                res->r_Base == 0x0F13) /* PNP0F13: PS/2 Port for PS/2-style Mice */
                return new ATMouse(cdp);
            return nullptr;
        }
    };

    const RegisterDriver<ATMouse_Driver> registerDriver;
} // unnamed namespace
