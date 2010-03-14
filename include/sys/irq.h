#include <sys/device.h>

#ifndef __IRQ_H__
#define __IRQ_H__

typedef void (*irqhandler_t)(device_t);

/*
 * Describes an IRQ in machine-independant context.
 */
struct IRQ {
	device_t dev;
	irqhandler_t handler;
};

int irq_register(unsigned int no, device_t dev, irqhandler_t handler);
void irq_handler(unsigned int no);

#endif /* __IRQ_H__ */
