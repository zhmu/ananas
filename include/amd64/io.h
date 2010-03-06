#include "types.h"

#ifndef __AMD64_IO_H__
#define __AMD64_IO_H__

/* XXX i386 PIC1/2 registers and locations */
#define PIC1_BASE		0x20
#define PIC1_CMD		(PIC1_BASE)
#define PIC1_DATA		(PIC1_BASE+1)
#define PIC2_BASE		0xa0
#define PIC2_CMD		(PIC2_BASE)
#define PIC2_DATA		(PIC2_BASE+1)

#define ICW1_ICW4		0x01	/* ICW4 */
#define ICW1_SINGLE		0x02	/* Single or cascade mode */
#define ICW1_INTERVAL4		0x04	/* Call address interval 4 (=8) */
#define ICW1_LEVEL		0x08	/* Level trigger (=edge) mode */
#define ICW1_INIT		0x10	/* Initialization mode */

#define ICW4_8086		0x01	/* 8086/8088 mode */
#define ICW4_AUTO		0x02	/* Auto (=normal) EOI */
#define ICW4_BUF_SLAVE		0x08	/* Buffered mode slave */
#define ICW4_BUF_MASTER		0x0c	/* Buffered mode master */
#define ICW4_SNFM		0x10	/* Not Special Fully Nested */

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

#endif /* __AMD64_IO_H__ */
