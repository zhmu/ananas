#include "i386/io.h"
#include "lib.h"

#define SIO_PORT 0x3f8

void
sio_init()
{
}

void
sio_putc(char c)
{
	asm(
		/* Poll the LSR to ensure we're not sending another character */
		"mov	%%ecx, %%edx\n"
		"addl	$5,%%edx\n"
"z1f:\n"
		"in		%%dx,%%al\n"
		"test	$0x20, %%al\n"
		"jz	 	z1f\n"
		/* Place the character in the data register */
		"mov	%%ecx, %%edx\n"
		"mov	%%ebx, %%eax\n"
		"outb	%%al, %%dx\n"
		"mov	%%ecx, %%edx\n"
		"mov	%%ecx, %%edx\n"
		"addl	$5,%%edx\n"
"z2f:\n"
		"in		%%dx,%%al\n"
		"test	$0x20, %%al\n"
		"jz	 	z2f\n"
	: : "b" (c), "c" (SIO_PORT));
	if (c == '\n')
		sio_putc('\r');
}

/* vim:set ts=2 sw=2: */
