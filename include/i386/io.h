#include "types.h"

#ifndef __I386_IO_H__
#define __I386_IO_H__

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

#endif /* __I386_IO_H__ */
