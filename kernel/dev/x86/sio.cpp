#include "kernel/console-driver.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "kernel/dev/tty.h"
#include "kernel/x86/io.h"
#include "kernel/x86/sio.h"

TRACE_SETUP;

namespace {

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

irq::IRQResult
SIO::OnIRQ()
{
	char ch = inb(sio_port + SIO_REG_DATA);

	OnInput(&ch, 1);
	return irq::IRQResult::Processed;
}

Result
SIO::Attach()
{
	void* res_io = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IO, 7);
	void* res_irq = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return RESULT_MAKE_FAILURE(ENODEV);

	sio_port = (uint32_t)(uintptr_t)res_io;

	/* SIO is so simple that a plain ISR will do */
	RESULT_PROPAGATE_FAILURE(
		irq::Register((uintptr_t)res_irq, this, IRQ_TYPE_ISR, *this)
	);

	/*
	 * Wire up the serial port for sensible defaults.
	 */
	outb(sio_port + SIO_REG_IER, 0);			/* Disables interrupts */
	outb(sio_port + SIO_REG_LCR, 0x80);	/* Enable DLAB */
	outb(sio_port + SIO_REG_DATA, 0xc);	/* Divisor low byte (9600 baud) */
	outb(sio_port + SIO_REG_IER,  0);		/* Divisor hi byte */
	outb(sio_port + SIO_REG_LCR, 3);			/* 8N1 */
	outb(sio_port + SIO_REG_FIFO, 0xc7);	/* Enable/clear FIFO (14 bytes) */
	outb(sio_port + SIO_REG_IER, 0x01);	/* Enable interrupts (recv only) */
	return Result::Success();
}

Result
SIO::Detach()
{
	panic("TODO");
	return Result::Success();
}

Result
SIO::Print(const char* buffer, size_t len)
{
	for(/* nothing */; len > 0; buffer++, len--) {
		while ((inb(sio_port + SIO_REG_LSR) & 0x20) == 0)
			/* nothing */;
		outb(sio_port + SIO_REG_DATA, *buffer);
	}
	return Result::Success();
}

struct SIO_Driver : public Ananas::ConsoleDriver
{
	SIO_Driver()
	 : ConsoleDriver("sio", 200)
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Ananas::Device* ProbeDevice() override
	{
		if (s_IsConsole)
			return nullptr;

		s_IsConsole = true;
		Ananas::ResourceSet resourceSet;
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_IO, 0x3f8, 7));
		resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_IRQ, 4, 0));
		return new SIO(Ananas::CreateDeviceProperties(resourceSet));
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		if (s_IsConsole)
			return nullptr;

		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_PNP_ID, 0);
		if (res != NULL && res->r_Base == 0x0501) /* PNP0501: 16550A-compatible COM port */
			return new SIO(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(SIO_Driver)

/* vim:set ts=2 sw=2: */
