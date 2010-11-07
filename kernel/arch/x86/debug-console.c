#include <ananas/debug-console.h>
#include <ananas/i386/io.h>
#include <ananas/x86/sio.h>
#include <machine/param.h> /* for KERNBASE */

#define DEBUGCON_IO 0x3f8	/* COM1 */

static int debugcon_bochs_console = 0;

void
debugcon_init()
{
	/*
	 * Checks for the bochs 0xe9 'hack', which is actually a very handy feature;
	 * it allows us to dump debugger output to the bochs console, which we will
	 * do in addition to the serial port (since we cannot read from it)
	 */
	outb(0x80, 0xff);
	if(inb(0xe9) == 0xe9) {
#ifdef __i386__
		/* Paging isn't set up in i386 yet... so this ugly hack is needed */
		*(int*)((addr_t)&debugcon_bochs_console - KERNBASE) = 1;
#else
		debugcon_bochs_console++;
#endif
	}

	/* Wire up the serial port */
	outb(DEBUGCON_IO + SIO_REG_IER,  0);			/* Disables interrupts */
	outb(DEBUGCON_IO + SIO_REG_LCR,  0x80);		/* Enable DLAB */
	outb(DEBUGCON_IO + SIO_REG_DATA, 3);			/* Divisor low byte (38400 baud) */
	outb(DEBUGCON_IO + SIO_REG_IER,  0);			/* Divisor hi byte */
	outb(DEBUGCON_IO + SIO_REG_LCR,  3);			/* 8N1 */
	outb(DEBUGCON_IO + SIO_REG_FIFO, 0xc7);		/* Enable/clear FIFO (14 bytes) */
}

void
debugcon_putch(int ch)
{
	if (debugcon_bochs_console)
		outb(0xe9, ch);

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
