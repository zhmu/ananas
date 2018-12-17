#include "kernel-md/io.h"
#include "kernel-md/sio.h"
#include "kernel/debug-console.h"

#define DEBUGCON_IO 0x3f8	/* COM1 */

void
debugcon_init()
{
	/* Wire up the serial port */
	outb(DEBUGCON_IO + SIO_REG_IER,  0);			/* Disables interrupts */
	outb(DEBUGCON_IO + SIO_REG_LCR,  0x80);		/* Enable DLAB */
	outb(DEBUGCON_IO + SIO_REG_DATA, 1);			/* Divisor low byte (115200 baud) */
	outb(DEBUGCON_IO + SIO_REG_IER,  0);			/* Divisor hi byte */
	outb(DEBUGCON_IO + SIO_REG_LCR,  3);			/* 8N1 */
	outb(DEBUGCON_IO + SIO_REG_FIFO, 0xc7);		/* Enable/clear FIFO (14 bytes) */
}

void
debugcon_putch(int ch)
{
	while((inb(DEBUGCON_IO + SIO_REG_LSR) & 0x20) == 0)
		/* wait for the transfer buffer to become empty */ ;
	outb(DEBUGCON_IO + SIO_REG_DATA, ch);
}

int
debugcon_getch()
{
	if ((inb(DEBUGCON_IO + SIO_REG_LSR) & 1) == 0)
		return 0;
	return inb(DEBUGCON_IO + SIO_REG_DATA);
}

/* vim:set ts=2 sw=2: */
