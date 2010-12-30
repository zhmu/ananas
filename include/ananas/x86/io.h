#ifndef __X86_IO_H__
#define __X86_IO_H__

#include <sys/types.h>

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
uint64_t rdtsc();

#endif /* __X86_IO_H__ */
