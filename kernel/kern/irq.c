#include <machine/interrupts.h>
#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/pcpu.h>
#include <ananas/trace.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/kdb.h>
#include "options.h"

TRACE_SETUP;

static struct IRQ irq[MAX_IRQS];
static struct IRQ_SOURCES irq_sources;
static spinlock_t spl_irq = SPINLOCK_DEFAULT_INIT;

/* Number of stray IRQ's that occur before reporting stops */
#define IRQ_MAX_STRAY_COUNT 10

void
irqsource_register(struct IRQ_SOURCE* source)
{
	register_t state = spinlock_lock_unpremptible(&spl_irq);

	/* Ensure no bogus ranges are being registered */
	KASSERT(source->is_count >= 1, "must register at least one irq");
	KASSERT(source->is_first + source->is_count < MAX_IRQS, "can't register beyond MAX_IRQS range");

	/* Ensure there will not be an overlap */
	if(!DQUEUE_EMPTY(&irq_sources)) {
		DQUEUE_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
			KASSERT(source->is_first + source->is_count < is->is_first || is->is_first + is->is_count < source->is_first, "overlap in interrupt ranges (have %u-%u, attempt to add %u-%u)", is->is_first, is->is_first + is->is_count, source->is_first, source->is_first + source->is_count);
		}
	}
	DQUEUE_ADD_TAIL(&irq_sources, source);

	/* Hook all IRQ's to this source - this saves having to look things up later */
	for(unsigned int n = 0; n < source->is_count; n++)
		irq[source->is_first + n].i_source = source;

	spinlock_unlock_unpremptible(&spl_irq, state);
}

void
irqsource_unregister(struct IRQ_SOURCE* source)
{
	register_t state = spinlock_lock_unpremptible(&spl_irq);

	KASSERT(!DQUEUE_EMPTY(&irq_sources), "no irq sources registered");
	/* Ensure our source is registered */
	int matches = 0;
	DQUEUE_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
		if (is != source)
			continue;
		matches++;
	}
	KASSERT(matches == 1, "irq source not registered");

	/* Walk through the IRQ's and ensure they do not use this source anymore */
	for (int i = 0; i < MAX_IRQS; i++) {
		if (irq[i].i_source != source)
			continue;
		irq[i].i_source = NULL;
		for (int n = 0; n < IRQ_MAX_HANDLERS; n++)
			KASSERT(irq[i].i_handler[n].h_func == NULL, "irq %u still registered to this source", i);
	}

	DQUEUE_REMOVE(&irq_sources, source);
	spinlock_unlock_unpremptible(&spl_irq, state);
}

/* Must be called with spl_irq held */
static struct IRQ_SOURCE*
irqsource_find(unsigned int num)
{
	if(!DQUEUE_EMPTY(&irq_sources)) {
		DQUEUE_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
			if (num < is->is_first || num >= is->is_first + is->is_count)
				continue;
			return is;
		}
	}
	return NULL;
}

errorcode_t
irq_register(unsigned int no, device_t dev, irqfunc_t func, void* context)
{
	if (no >= MAX_IRQS)
		return ANANAS_ERROR(BAD_RANGE);

	register_t state = spinlock_lock_unpremptible(&spl_irq);

	/*
	 * Look up the interrupt source; if we can't find it, it means this interrupt will
	 * never fire so we should refuse to register it.
	 */
	struct IRQ_SOURCE* is = irqsource_find(no);
	if (is == NULL) {
		spinlock_unlock_unpremptible(&spl_irq, state);
		return ANANAS_ERROR(NO_RESOURCE);
	}

	struct IRQ* i = &irq[no];
	i->i_source = is;

	/* Locate a free slot for the handler */
	int slot = 0;
	for (/* nothing */; i->i_handler[slot].h_func != NULL && slot < IRQ_MAX_HANDLERS; slot++)
		/* nothing */;
	if (slot == IRQ_MAX_HANDLERS) {
		spinlock_unlock_unpremptible(&spl_irq, state);
		return ANANAS_ERROR(FILE_EXISTS); /* XXX maybe make the error more generic? */
	}

	/* Found one, hook it up */
	struct IRQ_HANDLER* handler = &i->i_handler[slot];
	handler->h_device = dev;
	handler->h_func = func;
	handler->h_context = context;
	spinlock_unlock_unpremptible(&spl_irq, state);
	return ANANAS_ERROR_OK;
}

void
irq_unregister(unsigned int no, device_t dev, irqfunc_t func, void* context)
{
	KASSERT(no < MAX_IRQS, "interrupt %u out of range", no);

	register_t state = spinlock_lock_unpremptible(&spl_irq);
	struct IRQ* i = &irq[no];
	KASSERT(i->i_source != NULL, "interrupt %u has no source", no);

	int matches = 0;
	for (int slot = 0; slot < IRQ_MAX_HANDLERS; slot++) {
		struct IRQ_HANDLER* handler = &i->i_handler[slot];
		if (handler->h_device != dev || handler->h_func != func || handler->h_context != context)
			continue;

		/* Found a match; unregister it */
		handler->h_device = NULL;
		handler->h_func = NULL;
		handler->h_context = NULL;
		matches++;
	}
	spinlock_unlock_unpremptible(&spl_irq, state);

	KASSERT(matches > 0, "interrupt %u not registered", no);
}

void
irq_handler(unsigned int no)
{
	int cpuid = PCPU_GET(cpuid);
	struct IRQ* i = &irq[no];

	KASSERT(no < MAX_IRQS, "irq_handler: (CPU %u) impossible irq %u fired", cpuid, no);
	struct IRQ_SOURCE* is = i->i_source;
	KASSERT(is != NULL, "irq_handler(): irq %u without source fired", no);

	/* Try all handlers one by one until we have one that works */
	irqresult_t result = IRQ_RESULT_IGNORED;
	struct IRQ_HANDLER* handler = &i->i_handler[0];
	for (unsigned int slot = 0; result == IRQ_RESULT_IGNORED && slot < IRQ_MAX_HANDLERS; slot++, handler++) {
		if (handler->h_func == NULL)
			continue;
		result = handler->h_func(handler->h_device, handler->h_context);
	}

	/* If they IRQ wasn't handled, it is stray */
	if (result == IRQ_RESULT_IGNORED && i->i_straycount < IRQ_MAX_STRAY_COUNT) {
		kprintf("irq_handler(): (CPU %u) stray irq %u, ignored\n", cpuid, no);
		if (++i->i_straycount == IRQ_MAX_STRAY_COUNT)
			kprintf("irq_handler(): not reporting stray irq %u anymore\n", no);
	}

	/*
	 * Disable interrupts; as we were handling an interrupt, this means we've
	 * been interrupted. We'll want to clean up, because if another interrupt
	 * occurs, it'll just expand the current context - and if interrupts come
	 * quickly enough, we'll run out of stack and crash 'n burn.
	 */
	md_interrupts_disable();

	/* Acknowledge the interrupt once the handler is done */
	is->is_ack(is, no - is->is_first);

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
KDB_COMMAND(irq, NULL, "Display IRQ status")
{
	/* Note: no need to grab locks as the debugger runs with interrupts disabled */
	kprintf("Registered IRQ sources:\n");
	if(!DQUEUE_EMPTY(&irq_sources)) {
		DQUEUE_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
			kprintf(" IRQ %d..%d\n", is->is_first, is->is_first + is->is_count);
		}
	}

	kprintf("Registered handlers:\n");
	for (unsigned int no = 0; no < MAX_IRQS; no++) {
		struct IRQ* i = &irq[no];
		for (int slot = 0; slot < IRQ_MAX_HANDLERS; slot++) {
			struct IRQ_HANDLER* handler = &i->i_handler[slot];
			if (handler->h_func == NULL)
				continue;
			kprintf(" IRQ %d: device '%s' handler %p\n", no, (handler->h_device != NULL) ? handler->h_device->name : "<none>", handler->h_func);
		}
		if (i->i_straycount > 0)
			kprintf(" IRQ %d: stray count %d\n", no, i->i_straycount);
	}
}
#endif

/* vim:set ts=2 sw=2: */
