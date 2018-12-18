#include "kernel/console-driver.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/dev/tty.h"
#include "kernel-md/io.h"
#include "kernel-md/sio.h"

TRACE_SETUP;

namespace
{
    // XXX This is a horrible kludge to prevent attaching the console SIO twice...
    bool s_IsConsole = false;

    class SIO : public TTY, private irq::IHandler
    {
      public:
        using TTY::TTY;
        virtual ~SIO() = default;

        Result Attach() override;
        Result Detach() override;

        Result Print(const char* buffer, size_t len) override;

      private:
        irq::IRQResult OnIRQ() override;

        uint32_t sio_port;
    };

    irq::IRQResult SIO::OnIRQ()
    {
        char ch = inb(sio_port + SIO_REG_DATA);

        OnInput(&ch, 1);
        return irq::IRQResult::Processed;
    }

    Result SIO::Attach()
    {
        void* res_io = d_ResourceSet.AllocateResource(Resource::RT_IO, 7);
        if (res_io == nullptr)
            return RESULT_MAKE_FAILURE(ENODEV);

        sio_port = (uint32_t)(uintptr_t)res_io;
        if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
            return result;

        /*
         * Wire up the serial port for sensible defaults.
         */
        outb(sio_port + SIO_REG_IER, 0);     /* Disables interrupts */
        outb(sio_port + SIO_REG_LCR, 0x80);  /* Enable DLAB */
        outb(sio_port + SIO_REG_DATA, 0xc);  /* Divisor low byte (9600 baud) */
        outb(sio_port + SIO_REG_IER, 0);     /* Divisor hi byte */
        outb(sio_port + SIO_REG_LCR, 3);     /* 8N1 */
        outb(sio_port + SIO_REG_FIFO, 0xc7); /* Enable/clear FIFO (14 bytes) */
        outb(sio_port + SIO_REG_IER, 0x01);  /* Enable interrupts (recv only) */
        return Result::Success();
    }

    Result SIO::Detach()
    {
        panic("TODO");
        return Result::Success();
    }

    Result SIO::Print(const char* buffer, size_t len)
    {
        for (/* nothing */; len > 0; buffer++, len--) {
            while ((inb(sio_port + SIO_REG_LSR) & 0x20) == 0)
                /* nothing */;
            outb(sio_port + SIO_REG_DATA, *buffer);
        }
        return Result::Success();
    }

    struct SIO_Driver : public ConsoleDriver {
        SIO_Driver() : ConsoleDriver("sio", 200) {}

        const char* GetBussesToProbeOn() const override { return "acpi"; }

        Device* ProbeDevice() override
        {
            if (s_IsConsole)
                return nullptr;

            s_IsConsole = true;
            ResourceSet resourceSet;
            resourceSet.AddResource(Resource(Resource::RT_IO, 0x3f8, 7));
            resourceSet.AddResource(Resource(Resource::RT_IRQ, 4, 0));
            return new SIO(CreateDeviceProperties(resourceSet));
        }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            if (s_IsConsole)
                return nullptr;

            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PNP_ID, 0);
            if (res != NULL && res->r_Base == 0x0501) /* PNP0501: 16550A-compatible COM port */
                return new SIO(cdp);
            return nullptr;
        }
    };

    const RegisterDriver<SIO_Driver> registerDriver;

} // unnamed namespace

/* vim:set ts=2 sw=2: */
