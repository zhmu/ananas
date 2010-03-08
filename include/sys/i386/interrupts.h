#ifndef __I386_INTERRUPTS_H__
#define __I386_INTERRUPTS_H__

#define MAX_IRQS 16

#define SYSCALL_INT 0x30

#ifndef ASM
/* Syscall interrupt code */
extern void* syscall_int;
#endif

#endif /* __I386_INTERRUPTS_H__ */
