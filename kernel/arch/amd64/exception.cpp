#include <ananas/types.h>
#include <machine/param.h>
#include "kernel/gdb.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/process.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/signal.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "kernel-md/exceptions.h"
#include "kernel-md/frame.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/md.h"
#include "kernel-md/param.h"
#include "kernel-md/vm.h"
#include "../sys/syscall.h"
#include "options.h"

#define SIGNAL_DEBUG 0

namespace
{
    constexpr util::array<const char*, 20> exceptionNames{"Divide Error",
                                                          "Debug Exception",
                                                          "NMI",
                                                          "Breakpoint Exception",
                                                          "Overflow Exception",
                                                          "BOUND Range Exceeded Exception",
                                                          "Invalid Opcode",
                                                          "Device Not Available",
                                                          "Double Fault",
                                                          "Coprocessor Segment Overrun",
                                                          "Invalid TSS Exception",
                                                          "Segment Not Present",
                                                          "Stack Fault Exception",
                                                          "General Protection Fault",
                                                          "Page Fault Exception",
                                                          "15",
                                                          "x87 FPU Floating-Point Error",
                                                          "Alignment Check Exception",
                                                          "Machine Check Exception",
                                                          "SIMD Floating Point Exception"};

    int map_trapno_to_signal(int trapno)
    {
        switch (trapno) {
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

    void exception_nm(struct STACKFRAME* sf)
    {
        /*
         * This is the Device Not Available-exception, which will be triggered if
         * an FPU access is made while the task-switched-flag is set. We will
         * obtain the FPU state and bind it to the thread.
         */
        Thread* thread = PCPU_GET(curthread);
        KASSERT(thread != NULL, "curthread is NULL");
        PCPU_SET(fpu_context, &thread->md_fpu_ctx);

        /*
         * Clear the task-switched-flag; this is what triggered this exception in
         * the first place. Afterwards, we can safely restore the FPU context and
         * continue. The next instruction will no longer cause an exception, and
         * the new context will be saved by the scheduler IRQ.
         */
        __asm("clts\n"
              "frstor (%%rax)\n"
              :
              : "a"(&thread->md_fpu_ctx));
    }

    void exception_generic(struct STACKFRAME* sf)
    {
        const char* descr = sf->sf_trapno >= 0 && sf->sf_trapno < exceptionNames.size()
                                ? exceptionNames[sf->sf_trapno]
                                : "???";
        int userland = (sf->sf_cs & 3) == SEG_DPL_USER;

        kprintf(
            "(CPU %u) %s exception: %s (%u) at cs:rip = %x:%p\n", PCPU_GET(cpuid),
            userland ? "user land" : "kernel", descr, sf->sf_trapno, sf->sf_cs, sf->sf_rip);

        if (userland) {
            Thread* cur_thread = PCPU_GET(curthread);
            kprintf(
                "name='%s' pid=%d\n", cur_thread->t_name,
                cur_thread->t_process != NULL ? (int)cur_thread->t_process->p_pid : -1);
        }

        kprintf("rax=%p rbx=%p rcx=%p rdx=%p\n", sf->sf_rax, sf->sf_rbx, sf->sf_rcx, sf->sf_rdx);
        kprintf("rbp=%p rsi=%p rdi=%p rsp=%p\n", sf->sf_rbp, sf->sf_rsi, sf->sf_rdi, sf->sf_rsp);
        kprintf("r8=%p r9=%p r10=%p r11=%p\n", sf->sf_r8, sf->sf_r9, sf->sf_r10, sf->sf_r11);
        kprintf("r12=%p r13=%p r14=%p r15=%p\n", sf->sf_r12, sf->sf_r13, sf->sf_r14, sf->sf_r15);
        kprintf("errnum=%p, ss:esp = %x:%p\n", sf->sf_errnum, sf->sf_ss, sf->sf_rsp);
        if (sf->sf_trapno == EXC_PF) {
            /* Page fault; show offending address */
            addr_t fault_addr;
            __asm __volatile("mov %%cr2, %%rax" : "=a"(fault_addr));
            kprintf("fault address=%p (flags %x)\n", fault_addr, sf->sf_errnum);
        }

        if (userland) {
            // An userland thread misbehaved - send the signal; we'll fall out of the
            // handler and the thread should be able to catch the signal or we'll invoke
            // the default action
            signal::QueueSignal(*PCPU_GET(curthread), map_trapno_to_signal(sf->sf_trapno));
            return;
        }

#ifdef OPTION_GDB
        gdb_handle_exception(sf);
#endif
        panic("kernel exception");
    }

    void exception_pf(struct STACKFRAME* sf)
    {
        /* Obtain the fault address; it caused the page fault */
        addr_t fault_addr;
        __asm __volatile("mov %%cr2, %%rax" : "=a"(fault_addr));

        /* If interrupts were on, re-enable them - the fault addres is safe now */
        if (sf->sf_rflags & 0x200)
            md::interrupts::Enable();

        int flags = 0;
        if (sf->sf_errnum & EXC_PF_FLAG_RW)
            flags |= VM_FLAG_WRITE;
        else
            flags |= VM_FLAG_READ;

        // Let the VM code deal with the fault
        Thread* curthread = PCPU_GET(curthread);
        if (curthread != NULL && curthread->t_process != NULL) {
            if (auto result = curthread->t_process->p_vmspace->HandleFault(fault_addr, flags);
                result.IsSuccess())
                return; /* fault handeled */
        }

        // Couldn't be handled; chain through
        exception_generic(sf);
    }

} // unnamed namespace

extern "C" void exception(struct STACKFRAME* sf)
{
    switch (sf->sf_trapno) {
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

extern "C" void interrupt_handler(struct STACKFRAME* sf) { irq::InvokeHandler(sf->sf_trapno); }

extern "C" void lapic_interrupt_handler(struct STACKFRAME* sf)
{
    // XXX This assumes we have a static mapping from IRQ n -> IDT n+32. Is that always correct?
    irq::InvokeHandler(sf->sf_trapno - 32);
}

extern "C" void amd64_syscall(struct STACKFRAME* sf)
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

Result core_dump(Thread* t, struct STACKFRAME*);

Result md_core_dump(Thread& t)
{
    // Find the userland rsp
    size_t sf_offset = KERNEL_STACK_SIZE - sizeof(struct STACKFRAME);
    auto sf = reinterpret_cast<struct STACKFRAME*>((char*)t.md_kstack + sf_offset);
    return core_dump(&t, sf);
}

extern "C" void* md_invoke_signal(struct STACKFRAME* sf)
{
    KASSERT(sf->sf_rsp < KMEM_DIRECT_VA_START, "stack not in userland");

    Thread* thread = PCPU_GET(curthread);
    siginfo_t si;
    signal::Action* act = signal::DequeueSignal(*thread, si);
    if (act == nullptr)
        return nullptr;
    KASSERT(act->sa_handler != SIG_IGN, "attempt to handle ignored signal %d", si.si_signo);
    if (act->sa_handler == SIG_DFL) {
        signal::HandleDefaultSignalAction(si);
        return nullptr;
    }

    // Create a copy of the original stackframe - this is used by md_sigreturn()
    // to restore the complete context
    sf->sf_rsp -= sizeof(struct STACKFRAME);
    auto sf_copy = reinterpret_cast<struct STACKFRAME*>(sf->sf_rsp);
    memcpy(sf_copy, sf, sizeof(struct STACKFRAME));
    sf_copy->sf_rsp += sizeof(struct STACKFRAME);

    // Create the siginfo structure
    sf->sf_rsp -= sizeof(siginfo_t);
    auto siginfo = reinterpret_cast<siginfo_t*>(sf->sf_rsp);
    *siginfo = si;

    // Fake a return call
    sf->sf_rsp -= sizeof(register_t);
    auto retaddr = reinterpret_cast<register_t*>(sf->sf_rsp);
    *retaddr = USERLAND_SUPPORT_ADDR;

    // Create a stackframe structure to invoke the signal handler
    sf->sf_rsp -= sizeof(struct STACKFRAME);
    auto sf_handler = reinterpret_cast<struct STACKFRAME*>(sf->sf_rsp);
    memcpy(sf_handler, sf, sizeof(struct STACKFRAME));
#if SIGNAL_DEBUG
    kprintf(
        "sf_copy %p siginfo %p retaddr %p sf_handler=%p\n", sf_copy, siginfo, retaddr, sf_handler);
    kprintf(
        "sizeof()s = %d, %d, %d\n", (int)sizeof(siginfo_t), (int)sizeof(register_t),
        (int)sizeof(struct STACKFRAME));
#endif
    // We always call the function as if it's a sigaction; an ordinary handler
    // should just ignore the extra arguments
    sf_handler->sf_rdi = siginfo->si_signo; // signum
    sf_handler->sf_rsi = (addr_t)siginfo;   // siginfo_t
    sf_handler->sf_rdx = 0;                 // void*
    sf_handler->sf_rip = (addr_t)act->sa_handler;
    sf_handler->sf_rsp += sizeof(struct STACKFRAME);
    return sf_handler;
}

Result sys_sigreturn(Thread* t)
{
    // Find the userland rsp
    size_t sf_offset = KERNEL_STACK_SIZE - sizeof(struct STACKFRAME);
    auto sf = reinterpret_cast<struct STACKFRAME*>((char*)t->md_kstack + sf_offset);
#if SIGNAL_DEBUG
    kprintf(
        "sys_sigreturn: t %p, md_kstack %p, sf_offset %d => sf %p\n", t, t->md_kstack, sf_offset,
        sf);
    kprintf("sys_sigreturn: sf_rsp %p\n", sf->sf_rsp);
#endif
    /* XXX check sf->sf_rsp */
    auto sf_orig = reinterpret_cast<struct STACKFRAME*>((char*)sf->sf_rsp + sizeof(siginfo_t));
    /* XXX check sf_orig */

#if SIGNAL_DEBUG
    kprintf("%s: t=%p, rsp=%p sf_orig=%p\n", __func__, t, sf->sf_rsp, sf_orig);
#endif
    memcpy(sf, sf_orig, sizeof(struct STACKFRAME));

    // Ensure we return the previous return value of whatever we interrupted;
    // whatever signal handler we ran will likely have thrashed the value, so
    // we need to reload it
    return Result(sf->sf_rax, true);
}

/* vim:set ts=2 sw=2: */
