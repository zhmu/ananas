#include <sys/types.h>
#include <machine/frame.h>
#include <machine/interrupts.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <sys/x86/exceptions.h>
#include <sys/thread.h>
#include <sys/pcpu.h>
#include <sys/lib.h>

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
	PCPU_SET(fpu_context, &thread->md_fpu_ctx);

	/*
	 * Clear the task-switched-flag; this is what triggered this exception in
	 * the first place. Afterwards, we can safely restore the FPU context and
	 * continue. The next instruction will no longer cause an exception, and
	 * the new context will be saved by the scheduler IRQ.
	 */
	__asm(
			"clts\n"
			"frstor (%%eax)\n"
		: : "a" (&thread->md_fpu_ctx));
}

void
exception_generic(struct STACKFRAME* sf)
{
	const char* descr = x86_exception_name(sf->sf_trapno);
	int userland = (sf->sf_cs & 3) == SEG_DPL_USER;

	kprintf("(CPU %u) %s exception: %s (%u) at cs:eip = %x:%x\n",
		PCPU_GET(cpuid), userland ? "user land" : "kernel", descr,
		sf->sf_trapno, sf->sf_cs, sf->sf_eip);
	kprintf("eax=%x ebx=%x ecx=%x edx=%x\n", sf->sf_eax, sf->sf_ebx, sf->sf_ecx, sf->sf_edx);
	kprintf("esi=%x edi=%x ebp=%x\n", sf->sf_esi, sf->sf_edi, sf->sf_ebp);
	kprintf("ds=%x es=%x fs=%x gs=%x\n", sf->sf_ds, sf->sf_es, sf->sf_fs, sf->sf_gs);
	kprintf("possible ss:esp = %x:%x\n", sf->sf_ss, sf->sf_esp);
	if (userland) {
		/* A thread was naughty. Kill it */
		thread_exit();
		/* NOTREACHED */
	}
	panic("kernel exception");
}

void
exception_handler(struct STACKFRAME* sf)
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

/* vim:set ts=2 sw=2: */
