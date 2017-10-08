#include <machine/param.h>
#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/device.h"
#include "kernel/irq.h"
#include "kernel/kdb.h"
#include "kernel/lib.h"
#include "kernel/pcpu.h"
#include "kernel/trace.h"
#include "kernel-md/interrupts.h"
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
	LIST_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
		KASSERT(source->is_first + source->is_count < is->is_first || is->is_first + is->is_count < source->is_first, "overlap in interrupt ranges (have %u-%u, attempt to add %u-%u)", is->is_first, is->is_first + is->is_count, source->is_first, source->is_first + source->is_count);
	}
	LIST_APPEND(&irq_sources, source);

	/* Hook all IRQ's to this source - this saves having to look things up later */
	for(unsigned int n = 0; n < source->is_count; n++) {
		struct IRQ* i = &irq[source->is_first + n];
		i->i_source = source;
		/* Also a good time to initialize the semaphore */
		sem_init(&i->i_semaphore, 0);
	}

	spinlock_unlock_unpremptible(&spl_irq, state);
}

void
irqsource_unregister(struct IRQ_SOURCE* source)
{
	register_t state = spinlock_lock_unpremptible(&spl_irq);

	KASSERT(!LIST_EMPTY(&irq_sources), "no irq sources registered");
	/* Ensure our source is registered */
	int matches = 0;
	LIST_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
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

	LIST_REMOVE(&irq_sources, source);
	spinlock_unlock_unpremptible(&spl_irq, state);
}

static void
ithread(void* context)
{
	unsigned int no = (unsigned int)(uintptr_t)context;
	KASSERT(no < MAX_IRQS, "ithread for impossible irq %u", no);

	struct IRQ* i = &irq[no];
	struct IRQ_SOURCE* is = i->i_source;
	KASSERT(is != NULL, "ithread for irq %u without source fired", no);

	while(1) {
		sem_wait(&i->i_semaphore);

		/* XXX We do need a lock here */
		struct IRQ_HANDLER* handler = &i->i_handler[0];
		for (unsigned int slot = 0; slot < IRQ_MAX_HANDLERS; slot++, handler++) {
			if (handler->h_func == NULL || (handler->h_flags & IRQ_HANDLER_FLAG_SKIP))
				continue;
			if (handler->h_flags & IRQ_HANDLER_FLAG_THREAD)
				handler->h_func(handler->h_device, handler->h_context);
		}

		/* Unmask the IRQ, we can handle it again now that the IST is done */
		is->is_unmask(is, no - is->is_first);
	}
}

/* Must be called with spl_irq held */
static struct IRQ_SOURCE*
irqsource_find(unsigned int num)
{
	LIST_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
		if (num < is->is_first || num >= is->is_first + is->is_count)
			continue;
		return is;
	}
	return NULL;
}

errorcode_t
irq_register(unsigned int no, Ananas::Device* dev, irqfunc_t func, int type, void* context)
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
	handler->h_flags = 0;
	if (type != IRQ_TYPE_ISR)
		handler->h_flags |= IRQ_FLAG_THREAD;

	/*
	 * See if we need to create an IST; we can't call kthread_init() in a
	 * spinlock because it allocates memory. To cope, we do the following:
	 *
	 *  (1) Register the handler, but mark it as IRQ_HANDLER_FLAG_SKIP to avoid calling it
	 *  (2) Release the lock
	 *  (3) Create the IST
	 *  (4) Acquire the lock
	 *  (5)d Ad IRQ_FLAG_THREAD to the IRQ, and remove IRQ_HANDLER_FLAG_SKIP from the handler
	 */
	int create_thread = (i->i_flags & IRQ_HANDLER_FLAG_THREAD) == 0 && (handler->h_flags & IRQ_FLAG_THREAD);
	if (create_thread)
		handler->h_flags |= IRQ_HANDLER_FLAG_SKIP; /* (1) */

	if (create_thread) {
		spinlock_unlock_unpremptible(&spl_irq, state); /* (2) */

		/* (3) Create the thread */
		char thread_name[PAGE_SIZE];
		snprintf(thread_name, sizeof(thread_name) - 1, "irq-%d", no);
		thread_name[sizeof(thread_name) - 1] = '\0';
		kthread_init(&i->i_thread, thread_name, &ithread, (void*)(uintptr_t)no);
		thread_resume(&i->i_thread);

		/* XXX we should set a decent priority here */

		/* (4) Re-acquire the lock */
		state = spinlock_lock_unpremptible(&spl_irq);

		/* (5) Remove the IRQ_SKIP flag, add IRQ_FLAG_THREAD  */
		handler->h_flags &= ~IRQ_HANDLER_FLAG_SKIP;
		i->i_flags |= IRQ_FLAG_THREAD;
	}

	spinlock_unlock_unpremptible(&spl_irq, state);
	return ananas_success();
}

void
irq_unregister(unsigned int no, Ananas::Device* dev, irqfunc_t func, void* context)
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
		handler->h_flags = 0;
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
	i->i_count++;

	KASSERT(no < MAX_IRQS, "irq_handler: (CPU %u) impossible irq %u fired", cpuid, no);
	struct IRQ_SOURCE* is = i->i_source;
	KASSERT(is != NULL, "irq_handler(): irq %u without source fired", no);

	/*
	 * Try all handlers one by one until we have one that works - if we find a handler that is to be
	 * call from the IST, flag so we don't do that multiple times.
	 */
	int awake_thread = 0, handled = 0;
	struct IRQ_HANDLER* handler = &i->i_handler[0];
	for (unsigned int slot = 0; slot < IRQ_MAX_HANDLERS; slot++, handler++) {
		if (handler->h_func == NULL || (handler->h_flags & IRQ_HANDLER_FLAG_SKIP))
			continue;
		if ((handler->h_flags & IRQ_HANDLER_FLAG_THREAD) == 0) {
			/* plain old ISR */
			if (handler->h_func(handler->h_device, handler->h_context) == IRQ_RESULT_PROCESSED)
				handled++;
		} else {
			awake_thread = 1; /* need to invoke the IST */
		}
	}

	if (awake_thread) {
		/* Mask the interrupt source; the ithread will unmask it once done */
		is->is_mask(is, no - is->is_first);

		/* Awake the interrupt thread */
		sem_signal(&i->i_semaphore);
	} else if (!handled && i->i_straycount < IRQ_MAX_STRAY_COUNT) {
		/* If they IRQ wasn't handled, it is stray */
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
	LIST_FOREACH(&irq_sources, is, struct IRQ_SOURCE) {
		kprintf(" IRQ %d..%d\n", is->is_first, is->is_first + is->is_count);
	}

	kprintf("IRQ handlers:\n");
	for (unsigned int no = 0; no < MAX_IRQS; no++) {
		struct IRQ* i = &irq[no];
		int banner = 0;
		for (int slot = 0; slot < IRQ_MAX_HANDLERS; slot++) {
			struct IRQ_HANDLER* handler = &i->i_handler[slot];
			if (handler->h_func == NULL)
				continue;
			if (!banner) {
				kprintf(" IRQ %d flags %x count %u stray %d\n", no, i->i_flags, i->i_count, i->i_straycount);
				banner = 1;
			}
			kprintf("  device '%s' handler %p flags %x\n", (handler->h_device != NULL) ? handler->h_device->d_Name : "<none>", handler->h_func, handler->h_flags);
		}
		if (!banner && (i->i_flags != 0 || i->i_count > 0 || i->i_straycount > 0))
			kprintf(" IRQ %d flags %x count %u stray %d\n", no, i->i_flags, i->i_count, i->i_straycount);
	}
}
#endif

/* vim:set ts=2 sw=2: */
