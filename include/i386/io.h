#ifndef __I386_IO_H__
#define __I386_IO_H__

#define outb(port,a) asm("outb %0, %1" : : "a" (a), "id" (port))
#define outw(port,a) asm("outw %0, %1" : : "a" (a), "d" (port))
#define outl(port,a) asm("outl %0, %1" : : "a" (a), "d" (port))

#endif /* __I386_IO_H__ */
