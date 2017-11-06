#include <ananas/types.h>
#include "kernel/time.h"
#include "kernel/lock.h"
#include "kernel/pcpu.h"
#include "kernel/schedule.h"
#include "kernel/x86/smp.h" // XXX

#include "kernel/lib.h"

namespace Ananas {

namespace Time {

namespace {

// XXX Maybe using a spinlock here isn't such a good idea - we could benefit much more
//     from atomic add/subtract/compare...
spinlock_t time_lock;
tick_t ticks = 0;
struct timespec time_current;

// DateToSerialDayNumber() is inspired by
// http://howardhinnant.github.io/date_algorithms.html, days_from_civil()
time_t DateToSerialDayNumber(const struct tm& tm)
{
	int32_t d = tm.tm_mday;
	int32_t m = tm.tm_mon;
	int32_t y = tm.tm_year;
	if (m <= 2)
		y--;

	int32_t era = (y >= 0 ? y : y - 399) / 400;
	uint32_t yearOfEra = static_cast<uint32_t>(y - era * 400); // 0 .. 399
	uint32_t dayOfYear = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // 0 .. 365
	uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear; // 0 .. 1460

	return era * 146097 + static_cast<int32_t>(dayOfEra) - 719468;
}

void TMtoTS(const struct tm& tm, struct timespec& ts)
{
	ts.tv_sec = DateToSerialDayNumber(tm) * (24 * 60 * 60);
	ts.tv_sec += tm.tm_hour * (60 * 60);
	ts.tv_sec += tm.tm_min * 60;
	ts.tv_sec += tm.tm_sec;
	ts.tv_nsec = 0;
}

} // unnamed namespace

unsigned int GetPeriodicyInHz()
{
	// XXX make me configurable in some way
	return 100;
}

tick_t GetTicks()
{
	register_t state = spinlock_lock_unpremptible(&time_lock);
	auto cur_ticks = ticks;
	spinlock_unlock_unpremptible(&time_lock, state);
	return cur_ticks;
}

void SetTime(const struct tm& tm)
{
	struct timespec ts;
	TMtoTS(tm, ts);
	SetTime(ts);
}

void SetTime(const struct timespec& ts)
{
	spinlock_lock(&time_lock);
	time_current = ts;
	spinlock_unlock(&time_lock);
}

struct timespec GetTime()
{
	spinlock_lock(&time_lock);
	auto ts = time_current;
	spinlock_unlock(&time_lock);
	return ts;
}

void
OnTick()
{
	// This should only be called in the boot CPU

	// Increment system tick count
	register_t state = spinlock_lock_unpremptible(&time_lock);
	ticks++;

	// Update the timestamp - XXX we should synchronise every now and then with
	// the RTC. XXX we can use the TSC to get a much more accurate value than
	// this
	time_current.tv_nsec += 1000000000 / GetPeriodicyInHz();
	while (time_current.tv_nsec >= 1000000000) {
		time_current.tv_sec++;
		time_current.tv_nsec -= 1000000000;
	}

	spinlock_unlock_unpremptible(&time_lock, state);

	if (!scheduler_activated())
		return;

#ifdef OPTION_SMP
	smp_broadcast_schedule();
#else
	/*
	 * Timeslice is up -> next thread please; we can implement this
	 * by simply setting the 'want to reschedule' flag.
	 */
	thread_t* curthread = PCPU_GET(curthread);
	curthread->t_flags |= THREAD_FLAG_RESCHEDULE;
#endif

}

} // namespace Time
} // namespace Ananas

/* vim:set ts=2 sw=2: */
