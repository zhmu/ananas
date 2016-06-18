#include <machine/param.h>
#include <machine/thread.h>
#include <machine/interrupts.h>
#include <machine/vm.h>
#include <ananas/thread.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/syscall.h>
#include <ananas/vm.h>
#include <ananas/vmspace.h>

extern void* kernel_pagedir;
void clone_return();
void userland_trampoline();
void kthread_trampoline();

errorcode_t
md_thread_init(thread_t* t, int flags)
{
	/*
	 * Create the user stack page piece, if we are not cloning - otherwise, we'll
	 * copy the parent's stack instead.
	 */
	if ((flags & THREAD_ALLOC_CLONE) == 0) {
		vmarea_t* va;
		errorcode_t err = vmspace_mapto(t->t_vmspace, USERLAND_STACK_ADDR, 0, THREAD_STACK_SIZE, VM_FLAG_USER | VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_ALLOC, &va);
		ANANAS_ERROR_RETURN(err);
	}

	/* Create the kernel stack for this thread; this is fixed-length and always mapped */
	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);

	/* Fill out our MD fields */
	t->md_cr3 = KVTOP((addr_t)t->t_vmspace->vs_md_pagedir);
	t->md_rsp0 = (addr_t)t->md_kstack + KERNEL_STACK_SIZE;
	t->md_rsp = t->md_rsp0;
	t->md_rip = (addr_t)&userland_trampoline;

	/* initialize FPU state similar to what finit would do */
	t->md_fpu_ctx.fcw = 0x37f;
	t->md_fpu_ctx.ftw = 0xffff;
	return ANANAS_ERROR_OK;
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	/* Set up the thread context */
	t->md_rip = (addr_t)&kthread_trampoline;
	t->md_arg1 = (addr_t)kfunc;
	t->md_arg2 = (addr_t)arg;
	t->md_cr3 = KVTOP((addr_t)kernel_pagedir);

	/*
	 * Kernel threads share the kernel pagemap and thus need to map the kernel
	 * stack. We do not differentiate between kernel and userland stacks as
	 * no kernelthread ever runs userland code.
	 */
  t->md_kstack = kmalloc(KERNEL_STACK_SIZE);
  t->md_rsp = (addr_t)t->md_kstack + KERNEL_STACK_SIZE - 16;

	return ANANAS_ERROR_OK;
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
	kfree(t->md_kstack);
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
	thread->md_arg1 = entry;
}

void
md_thread_set_argument(thread_t* thread, addr_t arg)
{
	thread->md_arg2 = arg;
}

void
md_thread_clone(struct THREAD* t, struct THREAD* parent, register_t retval)
{
	KASSERT(PCPU_GET(curthread) == parent, "must clone active thread");

	/* Restore the thread's own page directory */
	t->md_cr3 = KVTOP((addr_t)t->t_vmspace->vs_md_pagedir);

	/*
	 * We need to copy the part from the parent's kernel stack that lets us
	 * return safely to the original caller. Note that there are two important
	 * aspects here:
	 *
	 * - A thread's kernel stack is always mapped, so we can always access it.
	 * - We can start at the top of the kernel stack; the reason is that this is the
	 *   only way to enter the syscall (t->md_rsp can change if we are are pre-empted
	 *   by interrupts and such, but that is fine as they do not influence the result)
	 *
	 * We use the amd64-specific SYSCALL functionality; this won't push anything
	 * on the stack by itself so all we have to do is count what
	 * syscall_handler() places on the stack:
	 *  sizeof(struct SYSCALL_ARGS) +  userland_rsp + rcx + r11 + rbx + rbp + r12..15
	 *
	 */
#define SYSCALL_FRAME_SIZE (sizeof(struct SYSCALL_ARGS) + 9 * 8)

	/* Copy the part of the parent kernel stack over; it'll always be mapped */
	memcpy(t->md_kstack + KERNEL_STACK_SIZE - SYSCALL_FRAME_SIZE, parent->md_kstack + KERNEL_STACK_SIZE - SYSCALL_FRAME_SIZE, SYSCALL_FRAME_SIZE);

	/* Handle return value */
	t->md_rsp -= SYSCALL_FRAME_SIZE;
	t->md_rip = (addr_t)&clone_return;
	t->md_arg1 = retval;
}

/* vim:set ts=2 sw=2: */
