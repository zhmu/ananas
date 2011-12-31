#include <ananas/device.h>

#ifndef __IRQ_H__
#define __IRQ_H__

typedef void (*irqhandler_t)(device_t);

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

/*
 * Describes an IRQ in machine-independant context.
 */
struct IRQ {
	struct IRQ_SOURCE*	i_source;
	device_t		i_dev;
	irqhandler_t		i_handler;
	int			i_straycount;
};

/*
 * Note: on registering an IRQ source, all interrupts are expected to be
 * masked.
 */
void irqsource_register(struct IRQ_SOURCE* source);

int irq_register(unsigned int no, device_t dev, irqhandler_t handler);
void irq_handler(unsigned int no);
void irq_dump();

#endif /* __IRQ_H__ */
