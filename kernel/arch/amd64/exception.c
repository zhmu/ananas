#include <ananas/types.h>
#include <machine/frame.h>
#include <machine/interrupts.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <ananas/x86/exceptions.h>
#include <ananas/thread.h>
#include <ananas/pcpu.h>
#include <ananas/irq.h>
#include <ananas/lib.h>

static void
exception_nm(struct STACKFRAME* sf)
{
	/*
	 * This is the Device Not Available-exception, which will be triggered if
	 * an FPU access is made while the task-switched-flag is set. We will
	 * obtain the FPU state and bind it to the thread.
	 */
	struct THREAD* thread = PCPU_GET(curthread);
	KASSERT(thread != NULL, "curthread is NULL");
	PCPU_SET(fpu_context, &thread->md_ctx.fpu);

	/*
	 * Clear the task-switched-flag; this is what triggered this exception in
	 * the first place. Afterwards, we can safely restore the FPU context and
	 * continue. The next instruction will no longer cause an exception, and
	 * the new context will be saved by the scheduler IRQ.
	 */
	__asm(
			"clts\n"
			"frstor (%%eax)\n"
		: : "a" (&thread->md_ctx.fpu));
}

static void
exception_generic(struct STACKFRAME* sf)
{
	const char* descr = x86_exception_name(sf->sf_trapno);
	int userland = (sf->sf_cs & 3) == SEG_DPL_USER;

	kprintf("(CPU %u) %s exception: %s (%u) at cs:rip = %x:%x\n",
		PCPU_GET(cpuid), userland ? "user land" : "kernel", descr,
		sf->sf_trapno, sf->sf_cs, sf->sf_rip);

	kprintf("rax=%p rbx=%p rcx=%p rdx=%p\n", sf->sf_rax, sf->sf_rbx, sf->sf_rcx, sf->sf_rdx);
	kprintf("rbp=%p rsi=%p rdi=%p rsp=%p\n", sf->sf_rbp, sf->sf_rsi, sf->sf_rdi, sf->sf_rsp);
	kprintf("r 8=%p r 9=%p r10=%p r11=%p\n", sf->sf_r8, sf->sf_r9, sf->sf_r10, sf->sf_r11);
	kprintf("r12=%p r13=%p r14=%p r15=%p\n", sf->sf_r12, sf->sf_r13, sf->sf_r14, sf->sf_r15);
	kprintf("errnum=%p, ss:esp = %x:%p\n", sf->sf_errnum, sf->sf_ss, sf->sf_sp);
	if (userland) {
		/* A thread was naughty. Kill it */
		thread_exit(THREAD_MAKE_EXITCODE(THREAD_TERM_FAULT, sf->sf_trapno));
		/* NOTREACHED */
	}
	panic("kernel exception");
}

void
exception(struct STACKFRAME* sf)
{
	switch(sf->sf_trapno) {
		case EXC_NM:
			exception_nm(sf);
			return;
		default:
			exception_generic(sf);
			return;
	}
}

void
interrupt(struct STACKFRAME* sf)
{
	irq_handler(sf->sf_trapno);
}

/* vim:set ts=2 sw=2: */
