#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/hal.h>
#include <machine/timer.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ofw.h>

#define HZ 100 /* XXX does not belong here */

static uint32_t cpu_dec_base = 0;
extern int scheduler_active;

void
decrementer_interrupt()
{
	/* schedule the next interrupt XXX should take current value into account! */
	__asm __volatile("mtdec %0" : : "r" (cpu_dec_base));

	if (scheduler_activated())
		schedule();
}

void
timer_init()
{
	/* Update time base to zero */
	__asm __volatile(
		"li %r0, 0\n"
		"mttbl %r0\n"
		"mttbu %r0\n"
		"mttbl %r0\n"
	);

	/* Update the decrementer to fire every 1/HZ seconds */
	uint32_t cpu_freq = hal_get_cpu_speed();
	cpu_dec_base = cpu_freq / HZ;

	/* Update the decrementer to fire every 1/HZ seconds */
	cpu_dec_base = cpu_freq / HZ;
	kprintf("cpu at %u MHz\n", cpu_freq / 1000000);
	__asm __volatile("mtdec %0" : : "r" (cpu_dec_base));

	/*
	 * Enable external interrupts; this will give us periodic interrupts.
	 */
	wrmsr(rdmsr() | MSR_EE);
}

/* vim:set ts=2 sw=2: */
