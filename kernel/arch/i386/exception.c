#include <ananas/types.h>
#include <machine/frame.h>
#include <machine/interrupts.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/param.h>
#include <ananas/x86/exceptions.h>
#include <ananas/thread.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/pcpu.h>
#include <ananas/vm.h>
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

	kprintf("(CPU %u) %s exception: %s (%u) at cs:eip = %04x:%08x\n",
		PCPU_GET(cpuid), userland ? "user land" : "kernel", descr,
		sf->sf_trapno, sf->sf_cs, sf->sf_eip);
	kprintf("eax=%08x ebx=%08x ecx=%08x edx=%08x\n", sf->sf_eax, sf->sf_ebx, sf->sf_ecx, sf->sf_edx);
	kprintf("esi=%08x edi=%08x ebp=%08x esp=%08x\n", sf->sf_esi, sf->sf_edi, sf->sf_ebp, userland ? sf->sf_sp : sf->sf_esp);
	kprintf("ds=%04x es=%04x fs=%04x gs=%04x ss=%04x\n", sf->sf_ds, sf->sf_es, sf->sf_fs, sf->sf_gs, userland ? sf->sf_ss : 0);
	if (sf->sf_trapno == EXC_PF) {
		/* Page fault; show offending address */
		addr_t fault_addr;
		__asm __volatile("mov %%cr2, %%eax" : "=a" (fault_addr));
		kprintf("fault address=%x (flags %x)\n", fault_addr, sf->sf_errnum);
	}
	if (userland) {
		/* A thread was naughty. Kill it */
		thread_exit(THREAD_MAKE_EXITCODE(THREAD_TERM_FAULT, sf->sf_trapno));
		/* NOTREACHED */
	}
	panic("kernel exception");
}

static void
exception_pf(struct STACKFRAME* sf)
{
	/* Obtain the fault address; it caused the page fault */
	addr_t fault_addr;
	__asm __volatile("mov %%cr2, %%eax" : "=a" (fault_addr));

	/*
	 * If the interrupt flag was on, restore it - we have the fault address
	 * so it can safely be overwritten.
	 */
	if (sf->sf_eflags & EFLAGS_IF)
		__asm __volatile("sti");

	int userland = (sf->sf_cs & 3) == SEG_DPL_USER;
	if (userland) {
		/*
		 * Userland pagefault - we should see if there is an adequate mapping for this
		 * thread
		 */
		int flags = 0;
		if (sf->sf_errnum & EXC_PF_FLAG_RW)
			flags |= VM_FLAG_WRITE;
		else
			flags |= VM_FLAG_READ;
		if ((sf->sf_errnum & EXC_PF_FLAG_P) == 0) { /* page not present */
			errorcode_t err = thread_handle_fault(PCPU_GET(curthread), fault_addr & ~(PAGE_SIZE - 1), flags);
			if (err == ANANAS_ERROR_NONE) {
				return; /* fault handeled */
			}
		}
	}

	/* Either not in userland or couldn't be handled; chain through */
	exception_generic(sf);
}

void
exception_handler(struct STACKFRAME* sf)
{
	switch(sf->sf_trapno) {
		case EXC_NM:
			exception_nm(sf);
			return;
		case EXC_PF:
			exception_pf(sf);
			return;
		default:
			exception_generic(sf);
			return;
	}
}

void
interrupt_handler(struct STACKFRAME* sf)
{
	irq_handler(sf->sf_trapno);
}

/* vim:set ts=2 sw=2: */
