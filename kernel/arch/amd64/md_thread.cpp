/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/page.h"
#include "kernel/pcpu.h"
#include "kernel/process.h"
#include "kernel/result.h"
#include "kernel/thread.h"
#include "kernel/vm.h"
#include "kernel/vmspace.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/frame.h"
#include "kernel-md/param.h"
#include "kernel-md/vm.h"
#include "../sys/syscall.h"

#include "kernel/kdb.h"

extern void* kernel_pagedir;
extern "C" {
void thread_trampoline();
}

namespace md::thread
{
    Result InitUserlandThread(Thread& t, int flags)
    {
        auto& proc = t.t_process;

        /*
         * Create the kernel stack for this thread; we'll grab a few pages for this
         * but we won't map all of them to ensure we can catch stack underflow
         * and overflow.
         */
        t.md_kstack_page = page_alloc_length(KERNEL_STACK_SIZE + PAGE_SIZE);
        t.md_kstack = kmem_map(
            t.md_kstack_page->GetPhysicalAddress() + PAGE_SIZE, KERNEL_STACK_SIZE,
            VM_FLAG_READ | VM_FLAG_WRITE);

        /* Set up a stackframe so that we can return to the kernel code */
        struct STACKFRAME* sf =
            (struct STACKFRAME*)((addr_t)t.md_kstack + KERNEL_STACK_SIZE - sizeof(*sf));
        memset(sf, 0, sizeof(*sf));
        sf->sf_ds = GDT_SEL_USER_DATA;
        sf->sf_es = GDT_SEL_USER_DATA;
        sf->sf_cs = GDT_SEL_USER_CODE + 3;
        sf->sf_ss = GDT_SEL_USER_DATA + 3;
        sf->sf_rflags = 0x200; /* IF */

        /* Fill out our MD fields */
        t.md_cr3 = KVTOP(reinterpret_cast<addr_t>(proc.p_vmspace.vs_md_pagedir));
        t.md_rsp = reinterpret_cast<addr_t>(sf);
        t.md_rsp0 = reinterpret_cast<addr_t>(t.md_kstack) + KERNEL_STACK_SIZE;
        t.md_rip = reinterpret_cast<addr_t>(&thread_trampoline);
        t.t_frame = sf;

        /* Initialize FPU state similar to what finit would do */
        t.md_fpu_ctx.fcw = 0x37f;
        t.md_fpu_ctx.ftw = 0xffff;

        return Result::Success();
    }

    void InitKernelThread(Thread& t, kthread_func_t kfunc, void* arg)
    {
        /*
         * Kernel threads share the kernel pagemap and thus need to map the kernel
         * stack. We do not differentiate between kernel and userland stacks as
         * no kernel thread ever runs userland code.
         */
        t.md_kstack_page = page_alloc_length(KERNEL_STACK_SIZE + PAGE_SIZE);
        t.md_kstack = kmem_map(
            t.md_kstack_page->GetPhysicalAddress() + PAGE_SIZE, KERNEL_STACK_SIZE,
            VM_FLAG_READ | VM_FLAG_WRITE);
        t.t_md_flags = THREAD_MDFLAG_FULLRESTORE;

        /* Set up a stackframe so that we can return to the kernel code */
        struct STACKFRAME* sf =
            (struct STACKFRAME*)((addr_t)t.md_kstack + KERNEL_STACK_SIZE - 16 - sizeof(*sf));
        memset(sf, 0, sizeof(*sf));
        sf->sf_ds = GDT_SEL_KERNEL_DATA;
        sf->sf_es = GDT_SEL_KERNEL_DATA;
        sf->sf_cs = GDT_SEL_KERNEL_CODE;
        sf->sf_ss = GDT_SEL_KERNEL_DATA;
        sf->sf_rflags = 0x200; /* IF */
        sf->sf_rip = (addr_t)kfunc;
        sf->sf_rdi = (addr_t)arg;
        sf->sf_rsp = ((addr_t)t.md_kstack + KERNEL_STACK_SIZE - 16);

        /* Set up the thread context */
        t.md_cr3 = KVTOP((addr_t)kernel_vmspace->vs_md_pagedir);
        t.md_rsp = (addr_t)sf;
        t.md_rip = (addr_t)&thread_trampoline;
    }

    void Free(Thread& t)
    {
        KASSERT(t.IsZombie(), "cannot free non-zombie thread");
        KASSERT(&::thread::GetCurrent() != &t, "cannot free current thread");

        /*
         * We only have to throw away the stack; everything else is part of
         * t.t_pages an will have been freed already (this is why we the thread must
         * be a zombie at this point)
         */
        kmem_unmap(t.md_kstack, KERNEL_STACK_SIZE);
        page_free(*t.md_kstack_page);
    }

    Thread& SwitchTo(Thread& new_thread, Thread& old_thread)
    {
        KASSERT(md::interrupts::Save() == 0, "interrupts must be disabled");
        KASSERT(!new_thread.IsZombie(), "cannot switch to a zombie thread");
        KASSERT(&new_thread != &old_thread, "switching to self?");
        KASSERT(new_thread.IsActive(), "new thread isn't running?");

        /*
         * Activate the corresponding kernel stack in the TSS and for the syscall
         * handler - we don't know how the thread is going to jump to kernel mode.
         */
        auto tss = reinterpret_cast<TSS*>(PCPU_GET(tss));
        tss->rsp0 = new_thread.md_rsp0;
        PCPU_SET(rsp0, new_thread.md_rsp0);

        // Switch FPU context
        __asm __volatile(
            "fxsave (%0)\n"
            "fxrstor (%1)\n"
             : : "r" (&old_thread.md_fpu_ctx),
                 "r" (&new_thread.md_fpu_ctx));

        // Activate the new_thread thread's page tables
        __asm __volatile("movq %0, %%cr3" : : "r"(new_thread.md_cr3));

        /*
         * This will only be called from kernel -> kernel transitions, and the
         * compiler sees it as an ordinary function call. This means we only have
         * to save the bare minimum of registers and we mark everything else as
         * clobbered.
         *
         * However, we can't ask GCC to preserve %rbp for us, so we must save/restore
         * it ourselves.
         */
        Thread* prev;
        __asm __volatile(
            "pushfq\n"
            "pushq %%rbp\n"
            "movq %%rsp,%[old_rsp]\n"  /* store old_thread %rsp */
            "movq %[new_rsp], %%rsp\n" /* write new_thread %rsp */
            "movq $1f, %[old_rip]\n"   /* write next %rip for old_thread thread */
            "pushq %[new_rip]\n"       /* load next %rip for new_thread thread */
            "ret\n"
            "1:\n" /* we are back */
            "popq %%rbp\n"
            "popfq\n"
            : /* out */
            [old_rsp] "=m"(old_thread.md_rsp), [old_rip] "=m"(old_thread.md_rip), "=b"(prev)
            : /* in */
            [new_rip] "a"(new_thread.md_rip), "b"(&old_thread), [new_rsp] "c"(new_thread.md_rsp)
            : /* clobber */ "memory", "cc" /* flags */, "rdx", "r8", "r9", "r10", "r11", "r12",
              "r13", "r14", "r15");
        return *prev;
    }

    void* MapThreadMemory(Thread& thread, void* ptr, size_t length, int write) { return ptr; }

    void Clone(Thread& t, Thread& parent, register_t retval)
    {
        KASSERT(&::thread::GetCurrent() == &parent, "must clone active thread");

        /* Restore the thread's own page directory */
        t.md_cr3 = KVTOP(reinterpret_cast<addr_t>(t.t_process.p_vmspace.vs_md_pagedir));

        /*
         * We need to copy the the stack frame so we can return return safely to the
         * original caller; this is always at the same position as we expect we'll
         * only clone after a syscall. It doesn't matter if any interrupts fire in
         * between as this will be cleaned up once we get here.
         */
        size_t sf_offset = KERNEL_STACK_SIZE - sizeof(struct STACKFRAME);
        struct STACKFRAME* sf_parent =
            reinterpret_cast<struct STACKFRAME*>((char*)parent.md_kstack + sf_offset);
        struct STACKFRAME* sf =
            reinterpret_cast<struct STACKFRAME*>((char*)t.md_kstack + sf_offset);
        memcpy(sf, sf_parent, sizeof(struct STACKFRAME));

        /*
         * Hook up the trap frame so we'll end up in the trampoline code, which will
         * fully restore the new thread's context.
         */
        t.t_frame = sf;
        t.md_rip = (addr_t)&thread_trampoline;
        t.md_rsp = (addr_t)sf;

        /* Update the stack frame with the new return value to the child */
        sf->sf_rax = retval;
    }

    void SetupPostExec(Thread& t, addr_t exec_addr, addr_t stack_addr)
    {
        struct STACKFRAME* sf = t.t_frame;
        sf->sf_rip = exec_addr;
        // x86_64 ELF ABI states that %rsp-8 needs to be 16-byte aligned; we'll
        // pass the original stack pointer containing the userland arguments
        // as %rdi which is the first function argument
        sf->sf_rsp = ((stack_addr - 8) & ~15) + 8;
        sf->sf_rdi = stack_addr;

        t.t_md_flags |= THREAD_MDFLAG_FULLRESTORE;
        t.md_rsp = (addr_t)sf;
        t.md_rip = (addr_t)&thread_trampoline;
    }
} // namespace md::thread

extern "C" Thread* md_GetCurrentThread()
{
    #if 0
    uint64_t rsp;
    __asm __volatile("movq %%rsp, %0" : "=r" (rsp));
    return reinterpret_cast<Thread*>(rsp & (KERNEL_STACK_SIZE - 1));
    #endif
    return PCPU_GET(curthread);
}
