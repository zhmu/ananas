#include <machine/interrupts.h>
#include <ananas/pcpu.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include "options.h"

static struct IRQ irq[MAX_IRQS];
static struct IRQ_SOURCES irq_sources;
static spinlock_t spl_irq = SPINLOCK_DEFAULT_INIT;

/* Number of stray IRQ's that occur before reporting stops */
#define IRQ_MAX_STRAY_COUNT 10

void
irqsource_register(struct IRQ_SOURCE* source)
{
	spinlock_lock(&spl_irq);
	/* Ensure there will not be an overlap */
	if(!DQUEUE_EMPTY(&irq_sources)) {
		DQUEUE_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
			KASSERT(!(source->is_first >= is->is_first && source->is_first + source->is_count <= is->is_first + is->is_count), "overlap in interrupt ranges (have %u-%u, attempt to add %u-%u)", is->is_first, is->is_first + is->is_count, source->is_first, source->is_first + source->is_count);
		}
	}
	DQUEUE_ADD_TAIL(&irq_sources, source);
	spinlock_unlock(&spl_irq);
}

static struct IRQ_SOURCE*
irqsource_find(unsigned int num)
{
	spinlock_lock(&spl_irq);
	if(!DQUEUE_EMPTY(&irq_sources)) {
		DQUEUE_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
			if (num < is->is_first || num > is->is_first + is->is_count)
				continue;
			spinlock_unlock(&spl_irq);
			return is;
		}
	}
	spinlock_unlock(&spl_irq);
	return NULL;
}

int
irq_register(unsigned int no, device_t dev, irqhandler_t handler)
{
	KASSERT(no < MAX_IRQS, "interrupt %u out of range", no);

	struct IRQ_SOURCE* is = irqsource_find(no);
	KASSERT(is != NULL, "source not found for interrupt %u", no); /* XXX for now */

	spinlock_lock(&spl_irq);
	KASSERT(irq[no].i_handler == NULL, "interrupt %u already registered", no);
	irq[no].i_source = is;
	irq[no].i_dev = dev;
	irq[no].i_handler = handler;
	irq[no].i_straycount = 0;
	spinlock_unlock(&spl_irq);
	return 1;
}

void
irq_handler(unsigned int no)
{
	int cpuid = PCPU_GET(cpuid);
	struct IRQ* i = &irq[no];
	struct IRQ_SOURCE* is = i->i_source;

	KASSERT(no < MAX_IRQS, "irq_handler: (CPU %u) impossible irq %u fired", cpuid, no);
	if (i->i_handler == NULL) {
		if (i->i_straycount < IRQ_MAX_STRAY_COUNT) {
			kprintf("irq_handler(): (CPU %u) stray irq %u, ignored\n", cpuid, no);
			if (++i->i_straycount == IRQ_MAX_STRAY_COUNT)
				kprintf("irq_handler(): not reporting stray irq %u anymore\n", no);
		}
		/* Find the IRQ source; this is necessary to acknowledge the stray IRQ */
		is = irqsource_find(no);
		KASSERT(is != NULL, "stray interrupt %u without source", no);
	} else {
		/* Call the interrupt handler */
		i->i_handler(i->i_dev);
	}

	/*
	 * Disable interrupts; as we were handling an interrupt, this means we've
	 * been interrupted. We'll want to clean up, because if another interrupt
	 * occurs, it'll just expand the current context - and if interrupts come
	 * quickly enough, we'll run out of stack and crash 'n burn.
	 */
	md_interrupts_disable();

	/* Acknowledge the interrupt once the handler is done */
	is->is_ack(i->i_source, no - is->is_first);

	/*
	 * Decrement the IRQ nesting counter; interrupts are disabled, so it's safe
	 * to do this - if the nesting counter reaches zero, we can do a reschedule
	 * if necessary.
	 */
	int irq_nestcount = PCPU_GET(nested_irq);
	KASSERT(irq_nestcount > 0, "IRQ nesting too low while in irq!");
	irq_nestcount--;
	PCPU_SET(nested_irq, irq_nestcount);

	/* If the IRQ handler resulted in a reschedule of the current thread, handle it */
	thread_t* curthread = PCPU_GET(curthread);
	if (irq_nestcount == 0 && THREAD_WANT_RESCHEDULE(curthread))
		schedule();
}

#ifdef OPTION_KDB
void
kdb_cmd_irq(int num_args, char** arg)
{
	kprintf("irq dump\n");
	struct IRQ* i = &irq[0];
	for (int no = 0; no < MAX_IRQS; i++, no++) {
		kprintf("irq %u: ", no);
		if (i->i_handler)
			kprintf("%s (%p)", i->i_dev != NULL ? i->i_dev->name : "?", i->i_handler);
		else
			kprintf("(unassigned)");
		if (i->i_straycount > 0)
			kprintf(", %u stray%s",
			 i->i_straycount,
			 (i->i_straycount == IRQ_MAX_STRAY_COUNT) ? ", no longer reporting" : "");
		kprintf("\n");
	}
}
#endif /* KDB */

/* vim:set ts=2 sw=2: */
