#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/timer.h>
#include <ananas/lib.h>
#include <ananas/schedule.h>
#include <ofw.h>

#define HZ 100 /* XXX does not belong here */

static uint32_t cpu_freq = 0;
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

	/* Obtain the running CPU */
	ofw_cell_t chosen = ofw_finddevice("/chosen");
	ofw_cell_t ihandle_cpu;
	ofw_getprop(chosen, "cpu", &ihandle_cpu, sizeof(ihandle_cpu));

	ofw_cell_t node = ofw_instance_to_package(ihandle_cpu);
	cpu_freq = 100000000; /* XXX default to 100MHz for psim */
	if (node != -1)
		ofw_getprop(node, "clock-frequency", &cpu_freq, sizeof(cpu_freq));

	/* Update the decrementer to fire every 1/HZ seconds */
	cpu_dec_base = cpu_freq / HZ;
	kprintf("cpu at %u MHz\n", cpu_freq, cpu_dec_base);
	__asm __volatile("mtdec %0" : : "r" (cpu_dec_base));

	/*
	 * Enable external interrupts; this will give us periodic interrupts.
	 */
	wrmsr(rdmsr() | MSR_EE);
}

/* vim:set ts=2 sw=2: */
