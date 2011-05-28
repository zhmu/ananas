#include <ananas/device.h>

#ifndef __IRQ_H__
#define __IRQ_H__

typedef void (*irqhandler_t)(device_t);

/*
 * Describes an IRQ in machine-independant context.
 */
struct IRQ {
	device_t	irq_dev;
	irqhandler_t	irq_handler;
	int		irq_straycount;
};

int irq_register(unsigned int no, device_t dev, irqhandler_t handler);
void irq_handler(unsigned int no);
void irq_dump();

#endif /* __IRQ_H__ */
