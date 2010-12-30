#include <ananas/x86/io.h>
#include <ananas/x86/pit.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>

#define TIMER_FREQ 1193182

#define HZ 100 /* XXX does not belong here */

void
x86_pit_init()
{
	uint16_t count = TIMER_FREQ / HZ;
	outb(PIT_MODE_CMD, PIT_CH_CHAN0 | PIT_MODE_3 | PIT_ACCESS_BOTH);
	outb(PIT_CH0_DATA, (count & 0xff));
	outb(PIT_CH0_DATA, (count >> 8));
}
	
uint32_t
x86_pit_calc_cpuspeed_mhz()
{
	/* Ensure the interrupt flag is enabled; otherwise this code never stops */
#ifdef __i386__
	uint32_t flags;
	__asm __volatile("pushfl; popl %%eax" : "=a" (flags) :);
#elif defined(__amd64__)
	uint64_t flags;
	__asm __volatile("pushfq; popq %%rax" : "=a" (flags) :);
#endif
	KASSERT((flags & 0x200) != 0, "interrupts must be enabled");

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
	return ((tsc_current - tsc_base) * HZ) / 1000000;
}

/* vim:set ts=2 sw=2: */
