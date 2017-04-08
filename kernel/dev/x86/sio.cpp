#include <ananas/x86/io.h>
#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/trace.h>
#include <ananas/tty.h>
#include <ananas/mm.h>
#include <ananas/x86/sio.h>

#define SIO_BUFFER_SIZE	16

TRACE_SETUP;

namespace {

class SIO : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations
{
public:
	using Device::Device;
	virtual ~SIO() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	ICharDeviceOperations* GetCharDeviceOperations() override
	{
		return this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	errorcode_t Read(void* data, size_t& len, off_t offset) override;
	errorcode_t Write(const void* data, size_t& len, off_t offset) override;

private:
	void OnIRQ();

	static irqresult_t IRQWrapper(Ananas::Device* device, void* context)
	{
		auto sio = static_cast<SIO*>(device);
		sio->OnIRQ();
		return IRQ_RESULT_PROCESSED;
	}

	uint32_t sio_port;
	/* Incoming data buffer */
	uint8_t sio_buffer[SIO_BUFFER_SIZE];
	uint8_t sio_buffer_readpos;
	uint8_t sio_buffer_writepos;
};

void
SIO::OnIRQ()
{
	uint8_t ch = inb(sio_port + SIO_REG_DATA);
	sio_buffer[sio_buffer_writepos] = ch;
	sio_buffer_writepos = (sio_buffer_writepos + 1) % SIO_BUFFER_SIZE;

	/* XXX signal consumers - this is a hack */
	if (console_tty != NULL && tty_get_inputdev(console_tty) == this)
		tty_signal_data();
}

errorcode_t
SIO::Attach()
{
	void* res_io = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IO, 7);
	void* res_irq = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	sio_port = (uint32_t)(uintptr_t)res_io;

	/* SIO is so simple that a plain ISR will do */
	errorcode_t err = irq_register((uintptr_t)res_irq, this, &IRQWrapper, IRQ_TYPE_ISR, NULL);
	ANANAS_ERROR_RETURN(err);

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
	return ananas_success();
}

errorcode_t
SIO::Detach()
{
	panic("TODO");
	return ananas_success();
}

errorcode_t
SIO::Write(const void* data, size_t& len, off_t offset)
{
	const char* ch = (const char*)data;

	for (size_t n = 0; n < len; n++, ch++) {
		while ((inb(sio_port + SIO_REG_LSR) & 0x20) == 0)
			/* nothing */;
		outb(sio_port + SIO_REG_DATA, *ch);
	}
	return ananas_success();
}

errorcode_t
SIO::Read(void* data, size_t& len, off_t offset)
{
	size_t returned = 0, left = len;
	char* buf = (char*)data;

	while (left-- > 0) {
		if (sio_buffer_readpos == sio_buffer_writepos)
			break;

		buf[returned++] = sio_buffer[sio_buffer_readpos];
		sio_buffer_readpos = (sio_buffer_readpos + 1) % SIO_BUFFER_SIZE;
	}
	len = returned;
	return ananas_success();
}

struct SIO_Driver : public Ananas::ConsoleDriver
{
	SIO_Driver()
	 : ConsoleDriver("sio", 200, CONSOLE_FLAG_INOUT)
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Ananas::Device* ProbeDevice() override
	{
		// XXX NOTYET - how do you probe SIO anyway?
		return nullptr;
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_PNP_ID, 0);
		if (res != NULL && res->r_Base == 0x0501) /* PNP0501: 16550A-compatible COM port */
			return new SIO(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(SIO_Driver)

/* vim:set ts=2 sw=2: */
