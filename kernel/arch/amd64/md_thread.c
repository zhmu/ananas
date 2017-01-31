#include <machine/param.h>
#include <machine/thread.h>
#include <machine/interrupts.h>
#include <machine/frame.h>
#include <machine/vm.h>
#include <ananas/thread.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/kmem.h>
#include <ananas/page.h>
#include <ananas/process.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/syscall.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>

extern void* kernel_pagedir;
void thread_trampoline();

errorcode_t
md_thread_init(thread_t* t, int flags)
{
	/* Create a stack if we aren't cloning - otherwise, we'll just copy the parent's stack instead */
	process_t* proc = t->t_process;
	if ((flags & THREAD_ALLOC_CLONE) == 0) {
		vmarea_t* va;
		errorcode_t err = vmspace_mapto(proc->p_vmspace, USERLAND_STACK_ADDR, 0, THREAD_STACK_SIZE, VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_ALLOC | VM_FLAG_MD, &va);
		ANANAS_ERROR_RETURN(err);
	}

	/*
	 * Create the kernel stack for this thread; we'll grab a few pages for this
	 * but we won't map all of them to ensure we can catch stack underflow
	 * and overflow.
	 */
	t->md_kstack_page = page_alloc_length(KERNEL_STACK_SIZE + PAGE_SIZE);
	t->md_kstack = kmem_map(page_get_paddr(t->md_kstack_page) + PAGE_SIZE, KERNEL_STACK_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);

	/* Set up a stackframe so that we can return to the kernel code */
	struct STACKFRAME* sf = (struct STACKFRAME*)((addr_t)t->md_kstack + KERNEL_STACK_SIZE - sizeof(*sf));
	memset(sf, 0, sizeof(*sf));
	sf->sf_ds = GDT_SEL_USER_DATA;
	sf->sf_es = GDT_SEL_USER_DATA;
	sf->sf_cs = GDT_SEL_USER_CODE + 3;
	sf->sf_ss = GDT_SEL_USER_DATA + 3;
	sf->sf_rflags = 0x200; /* IF */
	/* note that md_thread_set_entrypoint() / md_thread_set_argument() should be called! */
	sf->sf_rsp = ((addr_t)USERLAND_STACK_ADDR + THREAD_STACK_SIZE);

	/* Fill out our MD fields */
	t->md_cr3 = KVTOP((addr_t)proc->p_vmspace->vs_md_pagedir);
  t->md_rsp = (addr_t)sf;
	t->md_rsp0 = (addr_t)t->md_kstack + KERNEL_STACK_SIZE;
	t->md_rip = (addr_t)&thread_trampoline;
	t->t_frame = sf;

	/* Initialize FPU state similar to what finit would do */
	t->md_fpu_ctx.fcw = 0x37f;
	t->md_fpu_ctx.ftw = 0xffff;

	return ananas_success();
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	/*
	 * Kernel threads share the kernel pagemap and thus need to map the kernel
	 * stack. We do not differentiate between kernel and userland stacks as
	 * no kernelthread ever runs userland code.
	 */
	t->md_kstack_page = page_alloc_length(KERNEL_STACK_SIZE + PAGE_SIZE);
	t->md_kstack = kmem_map(page_get_paddr(t->md_kstack_page) + PAGE_SIZE, KERNEL_STACK_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	t->t_md_flags = THREAD_MDFLAG_FULLRESTORE;

	/* Set up a stackframe so that we can return to the kernel code */
	struct STACKFRAME* sf = (struct STACKFRAME*)((addr_t)t->md_kstack + KERNEL_STACK_SIZE - 16 - sizeof(*sf));
	memset(sf, 0, sizeof(*sf));
	sf->sf_ds = GDT_SEL_KERNEL_DATA;
	sf->sf_es = GDT_SEL_KERNEL_DATA;
	sf->sf_cs = GDT_SEL_KERNEL_CODE;
	sf->sf_ss = GDT_SEL_KERNEL_DATA;
	sf->sf_rflags = 0x200; /* IF */
	sf->sf_rip = (addr_t)kfunc;
	sf->sf_rdi = (addr_t)arg;
	sf->sf_rsp = ((addr_t)t->md_kstack + KERNEL_STACK_SIZE - 16);

	/* Set up the thread context */
	t->md_cr3 = KVTOP((addr_t)kernel_pagedir);
  t->md_rsp = (addr_t)sf;
	t->md_rip = (addr_t)&thread_trampoline;

	return ananas_success();
}

void
md_thread_free(thread_t* t)
{
	KASSERT(THREAD_IS_ZOMBIE(t), "cannot free non-zombie thread");
  KASSERT(PCPU_GET(curthread) != t, "cannot free current thread");

	/*
	 * We only have to throw away the stack; everything else is part of
	 * t->t_pages an will have been freed already (this is why we the thread must
	 * be a zombie at this point)
	 */
	kmem_unmap(t->md_kstack, KERNEL_STACK_SIZE);
	page_free(t->md_kstack_page);
}

thread_t*
md_thread_switch(thread_t* new, thread_t* old)
{
	KASSERT(md_interrupts_save() == 0, "interrupts must be disabled");
	KASSERT(!THREAD_IS_ZOMBIE(new), "cannot switch to a zombie thread");
	KASSERT(new != old, "switching to self?");
	KASSERT(THREAD_IS_ACTIVE(new), "new thread isn't running?");

	/*
	 * Activate the corresponding kernel stack in the TSS and for the syscall
	 * handler - we don't know how the thread is going to jump to kernel mode.
	 */
  struct TSS* tss = (struct TSS*)PCPU_GET(tss);
  tss->rsp0 = new->md_rsp0;
	PCPU_SET(rsp0, new->md_rsp0);

	/* Activate the new thread's page tables */
  __asm __volatile("movq %0, %%cr3" : : "r" (new->md_cr3));

	/*
	 * This will only be called from kernel -> kernel transitions, and the
	 * compiler sees it as an ordinary function call. This means we only have
	 * to save the bare minimum of registers and we mark everything else as
	 * clobbered.
	 *
	 * However, we can't ask GCC to preserve %rbp for us, so we must save/restore
	 * it ourselves.
	 */
	thread_t* prev;
	__asm __volatile(
		"pushfq\n"
		"pushq %%rbp\n"
		"movq %%rsp,%[old_rsp]\n" /* store old %rsp */
		"movq %[new_rsp], %%rsp\n" /* write new %rsp */
		"movq $1f, %[old_rip]\n" /* write next %rip for old thread */
		"pushq %[new_rip]\n" /* load next %rip for new thread */
		"ret\n"
	"1:\n" /* we are back */
		"popq %%rbp\n"
		"popfq\n"
	: /* out */
		[old_rsp] "=m" (old->md_rsp), [old_rip] "=m" (old->md_rip), "=b" (prev)
	: /* in */
		[new_rip] "a" (new->md_rip), "b" (old), [new_rsp] "c" (new->md_rsp)
	: /* clobber */ "memory",
		"cc" /* flags */,
		"rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
	);
	return prev;
}

void*
md_map_thread_memory(thread_t* thread, void* ptr, size_t length, int write)
{
	return ptr;
}

void
md_thread_set_entrypoint(thread_t* thread, addr_t entry)
{
	thread->t_frame->sf_rip = entry;
}

void
md_thread_set_argument(thread_t* thread, addr_t arg)
{
	thread->t_frame->sf_rdi = arg;
}

void
md_thread_clone(struct THREAD* t, struct THREAD* parent, register_t retval)
{
	KASSERT(PCPU_GET(curthread) == parent, "must clone active thread");

	/* Restore the thread's own page directory */
	t->md_cr3 = KVTOP((addr_t)t->t_process->p_vmspace->vs_md_pagedir);

	/*
	 * We need to copy the the stack frame so we can return return safely to the
	 * original caller; this is always at the same position as we expect we'll
	 * only clone after a syscall. It doesn't matter if any interrupts fire in
	 * between as this will be cleaned up once we get here.
	 */
	size_t sf_offset = KERNEL_STACK_SIZE - sizeof(struct STACKFRAME);
	struct STACKFRAME* sf_parent = parent->md_kstack + sf_offset;
	struct STACKFRAME* sf = t->md_kstack + sf_offset;
	memcpy(sf, sf_parent, sizeof(struct STACKFRAME));

	/*
	 * Hook up the trap frame so we'll end up in the trampoline code, which will
	 * fully restore the new thread's context.
	 */
	t->t_frame = sf;
	t->md_rip = (addr_t)&thread_trampoline;
	t->md_rsp = (addr_t)sf;

	/* Update the stack frame with the new return value to the child */
	sf->sf_rax = retval;
}

void
md_setup_post_exec(thread_t* t, addr_t exec_addr)
{
	struct STACKFRAME* sf = t->t_frame;
	sf->sf_rip = exec_addr;
	sf->sf_rdi = t->t_process->p_info_va;

	t->t_md_flags |= THREAD_MDFLAG_FULLRESTORE;
	t->md_rsp = (addr_t)sf;
	t->md_rip = (addr_t)&thread_trampoline;
}

/* vim:set ts=2 sw=2: */
