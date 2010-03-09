#include <types.h>
#include <machine/frame.h>
#include <pcpu.h>
#include <irq.h>
#include <lib.h>

void
exception(struct STACKFRAME* sf)
{
	kprintf("exception %u at cs:rip = %x:%p\n", sf->sf_trapno, sf->sf_cs, sf->sf_rip);
	kprintf("rax=%p rbx=%p rcx=%p rdx=%p\n", sf->sf_rax, sf->sf_rbx, sf->sf_rcx, sf->sf_rdx);
	kprintf("rbp=%p rsi=%p rdi=%p rsp=%p\n", sf->sf_rbp, sf->sf_rsi, sf->sf_rdi, sf->sf_rsp);
	kprintf("r 8=%p r 9=%p r10=%p r11=%p\n", sf->sf_r8, sf->sf_r9, sf->sf_r10, sf->sf_r11);
	kprintf("r12=%p r13=%p r14=%p r15=%p\n", sf->sf_r12, sf->sf_r13, sf->sf_r14, sf->sf_r15);
	kprintf("errnum=%p, ss:esp = %x:%p\n", sf->sf_errnum, sf->sf_ss, sf->sf_sp);

	panic("fatal exception");
}

void
interrupt(struct STACKFRAME* sf)
{
	irq_handler(sf->sf_trapno);
}

/* vim:set ts=2 sw=2: */
