#include <ananas/types.h>
#include <machine/frame.h>
#include <machine/interrupts.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/param.h> /* for PAGE_SIZE */
#include <ananas/x86/exceptions.h>
#include <ananas/thread.h>
#include <ananas/process.h>
#include <ananas/procinfo.h>
#include <ananas/pcpu.h>
#include <ananas/syscall.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>
#include <ananas/lib.h>
#include <ananas/gdb.h>
#include "options.h"

namespace {

int
map_trapno_to_signal(int trapno)
{
	switch(trapno) {
		case EXC_DB:
		case EXC_BP:
			return 5;
		case EXC_OF:
		case EXC_BR:
			return 16;
		case EXC_UD:
			return 4;
		case EXC_DE:
		case EXC_NM:
			return 8;
		case EXC_TS:
		case EXC_NP:
		case EXC_SS:
		case EXC_GP:
		case EXC_PF:
			return 11;
		case EXC_DF:
		case EXC_MF:
		default:
			return 7;
	}
}

void
exception_nm(struct STACKFRAME* sf)
{
	/*
	 * This is the Device Not Available-exception, which will be triggered if
	 * an FPU access is made while the task-switched-flag is set. We will
	 * obtain the FPU state and bind it to the thread.
	 */
	thread_t* thread = PCPU_GET(curthread);
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
			"frstor (%%rax)\n"
		: : "a" (&thread->md_fpu_ctx));
}

void
exception_generic(struct STACKFRAME* sf)
{
	const char* descr = x86_exception_name(sf->sf_trapno);
	int userland = (sf->sf_cs & 3) == SEG_DPL_USER;

	kprintf("(CPU %u) %s exception: %s (%u) at cs:rip = %x:%p\n",
		PCPU_GET(cpuid), userland ? "user land" : "kernel", descr,
		sf->sf_trapno, sf->sf_cs, sf->sf_rip);

	if (userland) {
		thread_t* cur_thread = PCPU_GET(curthread);
		kprintf("name='%s' pid=%d\n", cur_thread->t_name, cur_thread->t_process != NULL ? (int)cur_thread->t_process->p_pid : -1);
	}

	kprintf("rax=%p rbx=%p rcx=%p rdx=%p\n", sf->sf_rax, sf->sf_rbx, sf->sf_rcx, sf->sf_rdx);
	kprintf("rbp=%p rsi=%p rdi=%p rsp=%p\n", sf->sf_rbp, sf->sf_rsi, sf->sf_rdi, sf->sf_rsp);
	kprintf("r8=%p r9=%p r10=%p r11=%p\n", sf->sf_r8, sf->sf_r9, sf->sf_r10, sf->sf_r11);
	kprintf("r12=%p r13=%p r14=%p r15=%p\n", sf->sf_r12, sf->sf_r13, sf->sf_r14, sf->sf_r15);
	kprintf("errnum=%p, ss:esp = %x:%p\n", sf->sf_errnum, sf->sf_ss, sf->sf_rsp);
	if (sf->sf_trapno == EXC_PF) {
		/* Page fault; show offending address */
		addr_t fault_addr;
		__asm __volatile("mov %%cr2, %%rax" : "=a" (fault_addr));
		kprintf("fault address=%p (flags %x)\n", fault_addr, sf->sf_errnum);
	}

	if (userland) {
		/* A thread was naughty. Kill it XXX we should send the signal instead */
		thread_exit(THREAD_MAKE_EXITCODE(THREAD_TERM_SIGNAL, map_trapno_to_signal(sf->sf_trapno)));
		/* NOTREACHED */
	}

#ifdef OPTION_GDB
	gdb_handle_exception(sf);
#endif
	panic("kernel exception");
}

void
exception_pf(struct STACKFRAME* sf)
{
	/* Obtain the fault address; it caused the page fault */
	addr_t fault_addr;
	__asm __volatile("mov %%cr2, %%rax" : "=a" (fault_addr));

	/* If interrupts were on, re-enable them - the fault addres is safe now */
	if (sf->sf_rflags & 0x200)
		md_interrupts_enable();

	int flags = 0;
	if (sf->sf_errnum & EXC_PF_FLAG_RW)
		flags |= VM_FLAG_WRITE;
	else
		flags |= VM_FLAG_READ;
	if ((sf->sf_errnum & EXC_PF_FLAG_P) == 0) { /* page not present */
		thread_t* curthread = PCPU_GET(curthread);
		if (curthread != NULL && curthread->t_process != NULL) {
			errorcode_t err = vmspace_handle_fault(curthread->t_process->p_vmspace, fault_addr, flags);
			if (ananas_is_success(err))
				return; /* fault handeled */
		}
	}

	/* Either not in userland or couldn't be handled; chain through */
	exception_generic(sf);
}

} // unnamed namespace

extern "C" void
exception(struct STACKFRAME* sf)
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

extern "C" void
interrupt_handler(struct STACKFRAME* sf)
{
	irq_handler(sf->sf_trapno);
}

extern "C" void
amd64_syscall(struct STACKFRAME* sf)
{
	struct SYSCALL_ARGS sa;
	sa.number = sf->sf_rax;
	sa.arg1 = sf->sf_rdi;
	sa.arg2 = sf->sf_rsi;
	sa.arg3 = sf->sf_rdx;
	sa.arg4 = sf->sf_r10;
	sa.arg5 = sf->sf_r8;
	/*sa.arg6 = sf->sf_r9;*/
	sf->sf_rax = syscall(&sa);
}

/* vim:set ts=2 sw=2: */
