#include <ananas/x86/io.h>
#include <ananas/x86/pit.h>
#include <ananas/pcpu.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <machine/interrupts.h>

#define IRQ_PIT 0
#define TIMER_FREQ 1193182

#define HZ 100 /* XXX does not belong here */

extern int md_cpu_clock_mhz;
static uint64_t tsc_boot_time;

static void
x86_pit_irq()
{
	PCPU_SET(tickcount, PCPU_GET(tickcount) + 1);
	if (!scheduler_activated())
		return;
	
	/*
	 * Timeslice is up -> next thread please; we can implement this
	 * by simply setting the 'want to reschedule' flag.
	 */
	thread_t* curthread = PCPU_GET(curthread);
	curthread->flags |= THREAD_FLAG_RESCHEDULE;
}

/*
 * Obtains the number of milliseconds that have passed since boot. Because we
 * know the CPU speed in MHz as md_cpu_clock_mhz, it means one second means
 * '1000 * md_cpu_clock_mhz' ticks have passed.
 */
uint32_t
x86_get_ms_since_boot()
{
	uint64_t tsc = rdtsc() - tsc_boot_time;
	return (tsc / 1000) / md_cpu_clock_mhz;
}

void
x86_pit_init()
{
	uint16_t count = TIMER_FREQ / HZ;
	outb(PIT_MODE_CMD, PIT_CH_CHAN0 | PIT_MODE_3 | PIT_ACCESS_BOTH);
	outb(PIT_CH0_DATA, (count & 0xff));
	outb(PIT_CH0_DATA, (count >> 8));
	if (!irq_register(IRQ_PIT, NULL, x86_pit_irq))
		panic("cannot register timer irq");
}
	
uint32_t
x86_pit_calc_cpuspeed_mhz()
{
	/* Ensure the interrupt flag is enabled; otherwise this code never stops */
	KASSERT(md_interrupts_save(), "interrupts must be enabled");

	/*
	 * Interrupts are there, this means we can use the tick count to
	 * figure out the CPU speed; we use the following formula:
	 *
	 *  current - base     number of ticks to wait 1/HZ sec
	 * 
	 *  So to figure out the number of Hz's the CPU is, we'll have
	 *  (current-base)*HZ; 
	 */
	uint64_t tsc_base, tsc_current;
	uint32_t tickcount = PCPU_GET(tickcount);
	while (PCPU_GET(tickcount) == tickcount);	/* wait for next tick */
	tickcount += 2;
	tsc_base = rdtsc();
	while (PCPU_GET(tickcount) < tickcount);	/* wait for yet another tick */
	tsc_current = rdtsc();
	/* We use the tsc_current value as the boot time */
	tsc_boot_time = tsc_current;
	return ((tsc_current - tsc_base) * HZ) / 1000000;
}

void
delay(int ms)
{
	/* XXX For now, always delay using the TSC */

	/*
	 * Delaying using the TSC; we should already have initialized md_cpu_clock_mhz
	 * by now; this is the number of MHz the CPU clock is running at; so delaying
	 * one second will take 'md_cpu_clock_mhz * 1000000' ticks. This means
	 * delaying one millisecond is 1000 times faster, so we'll just have to
	 * wait 'ms * md_cpu_clock_mhz * 1000' ticks.
	 */
	uint64_t delay_in_ticks = ms * md_cpu_clock_mhz * 1000;
	uint64_t base = rdtsc();
	uint64_t end = base + delay_in_ticks;
	while (rdtsc() < end)
		/* wait for it */ ;
}

/* vim:set ts=2 sw=2: */
