#ifndef __IRQ_H__
#define __IRQ_H__

#include <ananas/util/list.h>
#include "kernel/thread.h"

namespace Ananas {
class Device;
}
class Result;

/* Return values for the IRQ handler */
enum class IRQResult {
	IR_Ignored,
	IR_Processed
};

/* Prototype of an IRQ handler function */
typedef IRQResult (*irqfunc_t)(Ananas::Device*, void*);

/*
 * Describes an IRQ source; this is generally an interrupt controller - every
 * source is responsible for a number of interrupts; these will be numbered
 * [ is_first .. is_first + is_count ]. Callbacks are always issued using the
 * relative interrupt number, i.e. using [ 0 .. is_count ].
 */
struct IRQSource : util::List<IRQSource>::NodePtr
{
	IRQSource(unsigned int first, unsigned int count)
	 : is_first(first), is_count(count)
	{
	}

	/* First interrupt number handled */
	unsigned int	is_first;
	/* Number of interrupts handled */
	unsigned int	is_count;

	/* Mask a given interrupt */
	virtual void	Mask(int) = 0;
	/* Unmask a given interrupt */
	virtual void	Unmask(int) = 0;
	/* Acknowledge a given interrupt */
	virtual void	Acknowledge(int) = 0;
};
typedef util::List<IRQSource> IRQSourceList;

#define IRQ_MAX_HANDLERS 4

/* Single IRQ handler */
struct IRQ_HANDLER {
	Ananas::Device*		h_device;
	void*			h_context;
	irqfunc_t		h_func;
	unsigned int		h_flags;
#define IRQ_HANDLER_FLAG_THREAD	(1 << 0)	/* invoke the handler from the IST */
#define IRQ_HANDLER_FLAG_SKIP	(1 << 1)	/* (internal use only) do not invoke the handler */
};

/*
 * Describes an IRQ in machine-independant context.
 */
struct IRQ {
	IRQSource*		i_source;
	struct IRQ_HANDLER	i_handler[IRQ_MAX_HANDLERS];
	unsigned int		i_count;
	unsigned int		i_straycount;
	unsigned int		i_flags;
#define IRQ_FLAG_THREAD	(1 << 0)	/* execute this handler from a thread */
	Thread			i_thread;
	Semaphore		i_semaphore{0};
};

/*
 * Note: on registering or removing an IRQ source, all interrupts are expected
 * to be masked.
 */
void irqsource_register(IRQSource& source);
void irqsource_unregister(IRQSource& source);

#define IRQ_TYPE_DEFAULT	IRQ_TYPE_IST
#define IRQ_TYPE_IST		0 /* use an IST to launch the handler */
#define IRQ_TYPE_ISR		1 /* do not launch the handler from a thread */
#define IRQ_TYPE_IPI		IRQ_TYPE_ISR
#define IRQ_TYPE_TIMER		IRQ_TYPE_ISR
Result irq_register(unsigned int no, Ananas::Device* dev, irqfunc_t func, int type, void* context);
void irq_unregister(unsigned int no, Ananas::Device* dev, irqfunc_t func, void* context);
void irq_handler(unsigned int no);
void irq_dump();

#endif /* __IRQ_H__ */
