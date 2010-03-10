#include <types.h>
#include <machine/frame.h>
#include <machine/interrupts.h>
#include <machine/thread.h>
#include <thread.h>
#include <x86/exceptions.h>
#include <pcpu.h>
#include <irq.h>
#include <lib.h>

static void
exception_nm(struct STACKFRAME* sf)
{
	/*
	 * This is the Device Not Available-exception, which will be triggered if
	 * an FPU access is made while the task-switched-flag is set. We will
	 * obtain the FPU state and bind it tot the thread.
	 */
	struct THREAD* thread = PCPU_GET(curthread);
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;
	PCPU_SET(fpu_context, &md->ctx.fpu);

	/*
	 * Clear the task-switched-flag; this is what triggered this exception in
	 * the first place. Afterwards, we can safely restore the FPU context and
	 * continue. The next instruction will no longer cause an exception, and
	 * the new context will be saved by the scheduler IRQ.
	 */
	__asm(
			"clts\n"
			"frstor (%%eax)\n"
		: : "a" (&md->ctx.fpu));
}

void
exception(struct STACKFRAME* sf)
{
	switch(sf->sf_trapno) {
		case EXC_NM:
			exception_nm(sf);
			return;
		default:
			kprintf("exception %u at cs:rip = %x:%p\n", sf->sf_trapno, sf->sf_cs, sf->sf_rip);
			kprintf("rax=%p rbx=%p rcx=%p rdx=%p\n", sf->sf_rax, sf->sf_rbx, sf->sf_rcx, sf->sf_rdx);
			kprintf("rbp=%p rsi=%p rdi=%p rsp=%p\n", sf->sf_rbp, sf->sf_rsi, sf->sf_rdi, sf->sf_rsp);
			kprintf("r 8=%p r 9=%p r10=%p r11=%p\n", sf->sf_r8, sf->sf_r9, sf->sf_r10, sf->sf_r11);
			kprintf("r12=%p r13=%p r14=%p r15=%p\n", sf->sf_r12, sf->sf_r13, sf->sf_r14, sf->sf_r15);
			kprintf("errnum=%p, ss:esp = %x:%p\n", sf->sf_errnum, sf->sf_ss, sf->sf_sp);
			panic("fatal exception");
			return;
	}
}

void
interrupt(struct STACKFRAME* sf)
{
	irq_handler(sf->sf_trapno);
}

/* vim:set ts=2 sw=2: */
