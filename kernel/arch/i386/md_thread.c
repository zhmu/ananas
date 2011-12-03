#include <ananas/types.h>
#include <machine/debug.h>
#include <machine/frame.h>
#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/smp.h>
#include <machine/interrupts.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>
#include <ananas/threadinfo.h>
#include <ananas/vm.h>
#include "options.h"

extern struct TSS kernel_tss;
void clone_return();
void userland_trampoline();
void kthread_trampoline();
extern uint32_t* kernel_pd;

errorcode_t
md_thread_init(thread_t* t)
{
	/* Create a pagedirectory and map the kernel pages in there */
	t->md_pagedir = kmalloc(PAGE_SIZE);
	memset(t->md_pagedir, 0, PAGE_SIZE);
	vm_map_kernel_addr(t->md_pagedir);

	/* Allocate stacks: one for the thread and one for the kernel */
	t->md_stack  = kmem_alloc(THREAD_STACK_SIZE / PAGE_SIZE);
	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);
	t->md_eip = (addr_t)&userland_trampoline;

	/* Perform adequate mapping for the userland stack */
	md_map_pages(t->md_pagedir, USERLAND_STACK_ADDR, KVTOP((addr_t)t->md_stack),  THREAD_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_USER);

	/* Fill our the %esp and %cr3 fields; we'll be started in supervisor mode, so use the appropriate stack */
	t->md_cr3 = KVTOP((addr_t)t->md_pagedir);
	t->md_esp0 = (addr_t)t->md_kstack + KERNEL_STACK_SIZE - 4;
	t->md_esp = t->md_esp0;

	/* initialize FPU state similar to what finit would do */
	t->md_fpu_ctx.cw = 0x37f;
	t->md_fpu_ctx.tw = 0xffff;

	t->next_mapping = 1048576;
	return ANANAS_ERROR_OK;
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	/* Set up the environment */
	t->md_eip = (addr_t)&kthread_trampoline;
	t->md_arg1 = (addr_t)kfunc;
	t->md_arg2 = (addr_t)arg;
	t->md_cr3 = KVTOP((addr_t)kernel_pd);

	/*
	 * Kernel threads share the kernel pagemap and thus need to map the kernel
	 * stack. We do not differentiate between kernel and userland stacks as
	 * no kernelthread ever runs userland code.
	 */
	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);
	t->md_esp = (addr_t)t->md_kstack + KERNEL_STACK_SIZE - 4;
	return ANANAS_ERROR_OK;
}

void
md_thread_free(thread_t* t)
{
	/*
	 * This is a royal pain: we are about to destroy the thread's mappings, but this also means
	 * we destroy the thread's stack - and it won't be able to continue. To prevent this, we
	 * can only be called from another thread (this means this code cannot be run until the
	 * thread to be destroyed is a zombie)
	 */
	KASSERT(t->flags & THREAD_FLAG_ZOMBIE, "cannot free non-zombie thread");
	KASSERT(PCPU_GET(curthread) != t, "cannot free current thread");

	/* Throw away the pagedir and stacks; they aren't in use so this will never hurt */
	if ((t->flags & THREAD_FLAG_KTHREAD) == 0) {
		vm_free_pagedir(t->md_pagedir);
		kmem_free(t->md_stack);
		kfree(t->md_kstack);
	} else {
		kfree(t->md_kstack);
	}
}

void
md_thread_switch(thread_t* new, thread_t* old)
{
	KASSERT(md_interrupts_save() == 0, "interrupts must be disabled");
	KASSERT((new->flags & THREAD_FLAG_ZOMBIE) == 0, "cannot switch to a zombie thread");
	KASSERT(new != old, "switching to self?");
	KASSERT((old->flags & THREAD_FLAG_ACTIVE) == 0, "old thread is running?");
	KASSERT((new->flags & THREAD_FLAG_ACTIVE), "new thread isn't running?");

	/* Activate the corresponding kernel stack in the TSS */
	struct TSS* tss = (struct TSS*)PCPU_GET(tss);
	tss->esp0 = new->md_esp0;

	/* Set debug registers */
	DR_SET(0, new->md_dr[0]);
	DR_SET(1, new->md_dr[1]);
	DR_SET(2, new->md_dr[2]);
	DR_SET(3, new->md_dr[3]);
	DR_SET(7, new->md_dr[7]);

	/* XXX Safety nets to ensure we won't restore a bogus stack */
	KASSERT(new->md_esp > (addr_t)new->md_kstack, "new=%p(%s) esp %p underflow (%p)", new, new->threadinfo->ti_args, new->md_esp, new->md_kstack);
	KASSERT(new->md_esp < ((addr_t)new->md_kstack + KERNEL_STACK_SIZE), "new=%p esp %p overrflow (%p)", new, new->md_esp, new->md_kstack + KERNEL_STACK_SIZE);

	/*
	 * Switch to the new thread; as this will only happen in kernel -> kernel
	 * contexts and even then, between a frame, it is enough to just
	 * switch the stack and mark all registers are being clobbered by assigning
	 * them to dummy outputs.
	 *
	 * Note that we can't force the compiler to reload %ebp this way, so we'll
	 * just save it ourselves.
	 */
	register register_t ebx, ecx, edx, esi, edi;
	__asm __volatile(
		"pushfl\n"
		"pushl %%ebp\n"
		"movl %%esp,%[old_esp]\n" /* store old %esp */
		"movl %[new_esp], %%esp\n" /* write new %esp */
		"movl $1f, %[old_eip]\n" /* write next %eip for old thread */
		"movl %[new_cr3], %%cr3\n"
		"pushl %[new_eip]\n" /* load next %eip for new thread */
		"ret\n"
	"1:\n" /* we are back */
		"popl %%ebp\n"
		"popfl\n"
	: /* out */
		[old_esp] "=m" (old->md_esp), [old_eip] "=m" (old->md_eip),
		/* clobbered registers */
		"=b" (ebx), "=c" (ecx), "=d" (edx), "=S" (esi), "=D" (edi)
	: /* in */
		[new_eip] "a" (new->md_eip), [new_esp] "b" (new->md_esp),
		[new_cr3] "c" (new->md_cr3)
	: /* clobber */"memory"
	);
}

void
md_thread_bootstrap(thread_t* new)
{
	KASSERT(md_interrupts_save() == 0, "interrupts must be disabled");
	KASSERT((new->flags & THREAD_FLAG_ACTIVE) != 0, "must switch to active thread");
	KASSERT((new->flags & THREAD_FLAG_KTHREAD) != 0, "must switch to kernel thread");
	KASSERT(PCPU_GET(curthread) == new, "must switch to active thread");

	__asm __volatile(
		"movl %[new_esp], %%esp\n" /* write new %esp */
		"jmp *%[new_eip]\n"
	: : /* in */
		[new_eip] "r" (new->md_eip), [new_esp] "b" (new->md_esp)
	);

	/* NOTREACHED */
}

void*
md_map_thread_memory(thread_t* thread, void* ptr, size_t length, int write)
{
	return ptr;
}

void*
md_thread_map(thread_t* thread, void* to, void* from, size_t length, int flags)
{
	KASSERT((flags & VM_FLAG_KERNEL) == 0, "attempt to map kernel memory");
	int num_pages = length / PAGE_SIZE;
	if (length % PAGE_SIZE > 0)
		num_pages++;
	if ((flags & VM_FLAG_DEVICE) == 0)
		from = (void*)KVTOP((addr_t)from);
	md_map_pages(thread->md_pagedir, (addr_t)to, (addr_t)from, num_pages, VM_FLAG_USER | flags);
	return to;
}

addr_t
md_thread_is_mapped(thread_t* thread, addr_t virt, int flags)
{
	return md_get_mapping(thread->md_pagedir, virt, flags);
}

errorcode_t
md_thread_unmap(thread_t* thread, addr_t addr, size_t length)
{
	int num_pages = length / PAGE_SIZE;
	if (length % PAGE_SIZE > 0)
		num_pages++;
	md_unmap_pages(thread->md_pagedir, addr, num_pages);
	return ANANAS_ERROR_OK;
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
md_thread_clone(thread_t* t, thread_t* parent, register_t retval)
{
	/* Restore the thread's own page directory */
	t->md_cr3 = KVTOP((addr_t)t->md_pagedir);

	/* Do not inherit breakpoints */
	t->md_dr[7] = DR7_LE | DR7_GE;

	/*
	 * Copy stack content; we need to copy the kernel stack over because we can
	 * obtain the stackframe from it, which allows us to return to the intended
	 * caller. The user stackframe is important as it will allow the new thread
	 * to return correctly from the clone syscall.
	 */
	void* ustack_src = vm_map_kernel((addr_t)parent->md_stack, THREAD_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ);
	void* ustack_dst = vm_map_kernel((addr_t)t     ->md_stack, THREAD_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	memcpy(ustack_dst, ustack_src, THREAD_STACK_SIZE);
	vm_unmap_kernel((addr_t)ustack_dst, THREAD_STACK_SIZE / PAGE_SIZE);
	vm_unmap_kernel((addr_t)ustack_src, THREAD_STACK_SIZE / PAGE_SIZE);

	/* Copy kernel stack over */
	memcpy(t->md_kstack, parent->md_kstack, KERNEL_STACK_SIZE);

	/*
	 * Handle returning to the new thread; this just cancels the system call
	 * and overrides the return value.
	 */
	t->md_esp = (addr_t)t->md_kstack + ((addr_t)parent->md_esp - (addr_t)parent->md_kstack);
	t->md_eip = (addr_t)&clone_return;
	t->md_arg1 = retval;
}

int
md_thread_peek_32(thread_t* thread, addr_t virt, uint32_t* val)
{
	addr_t phys = md_get_mapping(thread->md_pagedir, virt, VM_FLAG_READ);
	if (phys == 0)
		return 0;

	void* ptr = vm_map_kernel(phys, 1, VM_FLAG_READ);
	*val = *(uint32_t*)(ptr + (virt % PAGE_SIZE));
	vm_unmap_kernel((addr_t)ptr, 1);
	return 1;
}

/* vim:set ts=2 sw=2: */
