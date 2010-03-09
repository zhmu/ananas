#include <types.h>

void
outb(uint16_t port, uint8_t data) {
	__asm volatile ("outb %0, %1" : : "a" (data), "id" (port));
}

void
outw(uint16_t port, uint16_t data) {
	__asm volatile ("outw %0, %1" : : "a" (data), "id" (port));
}

void
outl(uint16_t port, uint32_t data) {
	__asm volatile ("outl %0, %1" : : "a" (data), "id" (port));
}

uint8_t
inb(uint16_t port)
{
	uint8_t a;
	__asm volatile ("inb %1, %0" : "=a" (a) : "id" (port));
	return a;
}

uint16_t
inw(uint16_t port)
{
	uint16_t a;
	__asm volatile ("inw %1, %0" : "=a" (a) : "id" (port));
	return a;
}

uint32_t
inl(uint16_t port)
{
	uint32_t a;
	__asm volatile ("inl %1, %0" : "=a" (a) : "id" (port));
	return a;
}
