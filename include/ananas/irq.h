#ifndef __IRQ_H__
#define __IRQ_H__

#include <ananas/device.h>
#include <ananas/thread.h>

/* Return values for the IRQ handler */
typedef enum {
	IRQ_RESULT_IGNORED,
	IRQ_RESULT_PROCESSED
} irqresult_t;

/* Prototype of an IRQ handler function */
typedef irqresult_t (*irqfunc_t)(device_t, void*);

/*
 * Describes an IRQ source; this is generally an interrupt controller - every
 * source is responsible for a number of interrupts; these will be numbered
 * [ is_first .. is_first + is_count ]. Callbacks are always issued using the
 * relative interrupt number, i.e. using [ 0 .. is_count ].
 */
struct IRQ_SOURCE {
	/* First interrupt number handled */
	unsigned int	is_first;
	/* Number of interrupts handled */
	unsigned int	is_count;
	/* Private data */
	void*           is_privdata;
	/* Queue fields */
	DQUEUE_FIELDS(struct IRQ_SOURCE);
	/* Mask a given interrupt */
	errorcode_t (*is_mask)(struct IRQ_SOURCE*, int);
	/* Unmask a given interrupt */
	errorcode_t (*is_unmask)(struct IRQ_SOURCE*, int);
	/* Acknowledge a given interrupt */
	void (*is_ack)(struct IRQ_SOURCE*, int);
};
DQUEUE_DEFINE(IRQ_SOURCES, struct IRQ_SOURCE);

#define IRQ_MAX_HANDLERS 4

/* Single IRQ handler */
struct IRQ_HANDLER {
	device_t		h_device;
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
	struct IRQ_SOURCE*	i_source;
	struct IRQ_HANDLER	i_handler[IRQ_MAX_HANDLERS];
	unsigned int		i_count;
	unsigned int		i_straycount;
	unsigned int		i_flags;
#define IRQ_FLAG_THREAD	(1 << 0)	/* execute this handler from a thread */
	thread_t		i_thread;
	semaphore_t		i_semaphore;
};

/*
 * Note: on registering or removing an IRQ source, all interrupts are expected
 * to be masked.
 */
void irqsource_register(struct IRQ_SOURCE* source);
void irqsource_unregister(struct IRQ_SOURCE* source);

#define IRQ_TYPE_DEFAULT	IRQ_TYPE_IST
#define IRQ_TYPE_IST		0 /* use an IST to launch the handler */
#define IRQ_TYPE_ISR		1 /* do not launch the handler from a thread */
#define IRQ_TYPE_IPI		IRQ_TYPE_ISR
#define IRQ_TYPE_TIMER		IRQ_TYPE_ISR
errorcode_t irq_register(unsigned int no, device_t dev, irqfunc_t func, int type, void* context);
void irq_unregister(unsigned int no, device_t dev, irqfunc_t func, void* context);
void irq_handler(unsigned int no);
void irq_dump();

#endif /* __IRQ_H__ */
