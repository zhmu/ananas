#include <ananas/error.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lock.h"
#include "kernel/time.h"
#include "kernel/trace.h"
#include "kernel/x86/io.h"
#include "kernel/lib.h"
#include "kernel-md/interrupts.h"

#define RTC_DEBUG 0

TRACE_SETUP;

namespace {

const int reg_second = 0x00;	/* bcd 0..59 */
const int reg_minute = 0x02;	/* bcd 0..59 */
const int reg_hour = 0x04;	/* bcd 0..23 */
const int reg_day = 0x07;	/* bcd 1..31 */
const int reg_month = 0x08;	/* bcd 1..12 */
const int reg_year = 0x09;	/* bcd 0..99 */
const int reg_statusA = 0x0a;
const int reg_statusA_updating = (1 << 7);

inline uint8_t
BCDToU8(uint8_t bcd)
{
	return (bcd & 0x0f) + (bcd >> 4) * 10;
}

class ATRTC : public Ananas::Device, private Ananas::IDeviceOperations
{
public:
	using Device::Device;
	virtual ~ATRTC() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

protected:
	uint8_t ReadRegister(int reg);

private:
	int atrtc_ioport;
	Spinlock atrtc_lock;
};

uint8_t
ATRTC::ReadRegister(int reg)
{
	SpinlockUnpremptibleGuard g(atrtc_lock);
	outb(atrtc_ioport, reg);
	return inb(atrtc_ioport + 1);
}

errorcode_t
ATRTC::Attach()
{
	void* res_io = d_ResourceSet.AllocateResource(Ananas::Resource::RT_IO, 2);
	if (res_io == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	atrtc_ioport = (uintptr_t)res_io;

	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	/* Ensure a time-update isn't active */
	while (ReadRegister(reg_statusA) & reg_statusA_updating)
		/* nothing */;

	/* RTC isn't updating - we have some amount of time to read it */
	{
		register_t state = md_interrupts_save();
		md_interrupts_disable();
		tm.tm_year = BCDToU8(ReadRegister(reg_year));
		tm.tm_mon = BCDToU8(ReadRegister(reg_month));
		tm.tm_mday = BCDToU8(ReadRegister(reg_day));
		tm.tm_hour = BCDToU8(ReadRegister(reg_hour));
		tm.tm_min = BCDToU8(ReadRegister(reg_minute));
		tm.tm_sec = BCDToU8(ReadRegister(reg_second));
		tm.tm_year += 1900;
		if (tm.tm_year < 1980)
			tm.tm_year += 100; // start at 2000 for years < 1980
		md_interrupts_restore(state);
	}
	Ananas::Time::SetTime(tm);

#if RTC_DEBUG
	Printf("date: %04d/%02d/%02d", tm.tm_year, tm.tm_mon, tm.tm_mday);
	Printf("time: %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
#endif

	return ananas_success();
}

errorcode_t
ATRTC::Detach()
{
	panic("detach");
	return ananas_success();
}

struct ATRTC_Driver : public Ananas::Driver
{
	ATRTC_Driver()
	 : Driver("atrtc")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "acpi";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_PNP_ID, 0);
		if (res != NULL && res->r_Base == 0x0b00) /* PNP0B00: AT real-time clock */
			return new ATRTC(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(ATRTC_Driver)

/* vim:set ts=2 sw=2: */
