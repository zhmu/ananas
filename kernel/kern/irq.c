#include <machine/interrupts.h>
#include <ananas/pcpu.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include "options.h"

static struct IRQ irq[MAX_IRQS];

/* Number of stray IRQ's that occur before reporting stops */
#define IRQ_MAX_STRAY_COUNT 10

void
irq_init()
{
	memset(irq, 0, sizeof(struct IRQ) * MAX_IRQS);
}

int
irq_register(unsigned int no, device_t dev, irqhandler_t handler)
{
	KASSERT(no < MAX_IRQS, "interrupt %u out of range", no);

	if (irq[no].irq_handler != NULL)
		return 0;

	irq[no].irq_dev = dev;
	irq[no].irq_handler = handler;
	irq[no].irq_straycount = 0;
	return 1;
}

void
irq_handler(unsigned int no)
{
	int cpuid = PCPU_GET(cpuid);
	KASSERT(no < MAX_IRQS, "irq_handler: (CPU %u) impossible irq %u fired", cpuid, no);
	if (irq[no].irq_handler == NULL) {
		if (irq[no].irq_straycount >= IRQ_MAX_STRAY_COUNT)
			return;
		kprintf("irq_handler(): (CPU %u) stray irq %u, ignored\n", cpuid, no);
		if (++irq[no].irq_straycount == IRQ_MAX_STRAY_COUNT)
			kprintf("irq_handler(): not reporting stray irq %u anymore\n", no);
		return;
	}

	irq[no].irq_handler(irq[no].irq_dev);

	/* If the IRQ handler resulted in a reschedule of the current thread, handle it */
	thread_t* curthread = PCPU_GET(curthread);
	if (curthread != NULL && curthread->flags & THREAD_FLAG_RESCHEDULE) {
		schedule();
	}
}

#ifdef OPTION_KDB
void
kdb_cmd_irq(int num_args, char** arg)
{
	kprintf("irq dump\n");
	for (int no = 0; no < MAX_IRQS; no++) {
		kprintf("irq %u: ", no);
		if (irq[no].irq_handler)
			kprintf("%s (%p)", irq[no].irq_dev != NULL ? irq[no].irq_dev->name : "?", irq[no].irq_handler);
		else
			kprintf("(unassigned)");
		if (irq[no].irq_straycount > 0)
			kprintf(", %u stray%s",
			 irq[no].irq_straycount,
			 (irq[no].irq_straycount == IRQ_MAX_STRAY_COUNT) ? ", no longer reporting" : "");
		kprintf("\n");
	}
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
