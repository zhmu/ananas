#include <ananas/types.h>
#include <machine/debug.h>
#include <machine/frame.h>
#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/smp.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>
#include <ananas/vm.h>
#include "options.h"

extern struct TSS kernel_tss;
void clone_return();
extern uint32_t* kernel_pd;

static errorcode_t
md_thread_setup(thread_t* t)
{
	memset(t->md_pagedir, 0, PAGE_SIZE);
	vm_map_kernel_addr(t->md_pagedir);

	/* Perform adequate mapping for the userland stack */
	md_map_pages(t->md_pagedir, USERLAND_STACK_ADDR, KVTOP((addr_t)t->md_stack),  THREAD_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_USER);
	md_map_pages(t->md_pagedir, KERNEL_STACK_ADDR,   KVTOP((addr_t)t->md_kstack), KERNEL_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_KERNEL);
	t->md_kstack_ptr = NULL;

	/* Fill out the thread's registers - anything not here will be zero */ 
	t->md_ctx.esp  = (addr_t)USERLAND_STACK_ADDR + THREAD_STACK_SIZE;
	t->md_ctx.esp0 = (addr_t)KERNEL_STACK_ADDR + KERNEL_STACK_SIZE - 4;
	t->md_ctx.cs = GDT_SEL_USER_CODE + SEG_DPL_USER;
	t->md_ctx.ds = GDT_SEL_USER_DATA + SEG_DPL_USER;
	t->md_ctx.es = GDT_SEL_USER_DATA + SEG_DPL_USER;
	t->md_ctx.ss = GDT_SEL_USER_DATA + SEG_DPL_USER;
	t->md_ctx.cr3 = KVTOP((addr_t)t->md_pagedir);
	t->md_ctx.eflags = EFLAGS_IF;
	t->md_ctx.dr[7] = DR7_LE | DR7_GE;

	/* initialize FPU state similar to what finit would do */
	t->md_fpu_ctx.cw = 0x37f;
	t->md_fpu_ctx.tw = 0xffff;

	t->next_mapping = 1048576;
	return ANANAS_ERROR_OK;
}

errorcode_t
md_thread_init(thread_t* t)
{
	/* Create a pagedirectory and map the kernel pages in there */
	t->md_pagedir = kmalloc(PAGE_SIZE);

	/* Allocate stacks: one for the thread and one for the kernel */
	t->md_stack  = kmem_alloc(THREAD_STACK_SIZE / PAGE_SIZE);
	t->md_kstack = kmem_alloc(KERNEL_STACK_SIZE / PAGE_SIZE);

	return md_thread_setup(t);
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	/* Set up the environment */
	t->md_ctx.cs = GDT_SEL_KERNEL_CODE;
	t->md_ctx.ds = GDT_SEL_KERNEL_DATA;
	t->md_ctx.es = GDT_SEL_KERNEL_DATA;
	t->md_ctx.fs = GDT_SEL_KERNEL_PCPU;
	t->md_ctx.eip = (addr_t)kfunc;
	t->md_ctx.cr3 = KVTOP((addr_t)kernel_pd);
	t->md_ctx.eflags = EFLAGS_IF;
	t->md_ctx.dr[7] = DR7_LE | DR7_GE;

	/*
	 * Kernel threads share the kernel pagemap and thus need to map the kernel
	 * stack. We do not differentiate between kernel and userland stacks as
	 * no kernelthread ever runs userland code.
	 */
	t->md_kstack_ptr = kmalloc(KERNEL_STACK_SIZE / PAGE_SIZE);
	t->md_ctx.esp0 = (addr_t)t->md_kstack_ptr + KERNEL_STACK_SIZE - 4;
	t->md_ctx.esp = t->md_ctx.esp0;

	/*
	 * Now, push 'arg' on the stack, as i386 passes arguments by the stack. Note that
	 * we must first place the value and then update esp0 because the interrupt code
	 * heavily utilized the stack, and the -= 4 protects our value from being
 	 * destroyed.
	 */
	*(uint32_t*)t->md_ctx.esp0 = (uint32_t)arg;
	t->md_ctx.esp0 -= 4;
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
		kmem_free(t->md_kstack);
	} else {
		kfree(t->md_kstack_ptr);
	}
}

void
md_thread_switch(thread_t* new, thread_t* old)
{
	struct CONTEXT* ctx_new = (struct CONTEXT*)&new->md_ctx;

	/* Force interrupts off - we cannot have any interruptions */
	__asm __volatile("cli");

	/* Activate this context as the current CPU context */
	PCPU_SET(context, ctx_new);

	/* Activate the corresponding kernel stack in the TSS */
	struct TSS* tss = (struct TSS*)PCPU_GET(tss);
	tss->esp0 = ctx_new->esp0;

	/* Set debug registers */
	DR_SET(0, ctx_new->dr[0]);
	DR_SET(1, ctx_new->dr[1]);
	DR_SET(2, ctx_new->dr[2]);
	DR_SET(3, ctx_new->dr[3]);
	DR_SET(7, ctx_new->dr[7]);

	/* Go! */
	md_restore_ctx(ctx_new);
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
	md_map_pages(thread->md_pagedir, (addr_t)to, KVTOP((addr_t)from), num_pages, VM_FLAG_USER | flags);
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
	thread->md_ctx.eip = entry;
}

void
md_thread_set_argument(thread_t* thread, addr_t arg)
{
	thread->md_ctx.esi = arg;
}

void
md_thread_clone(thread_t* t, thread_t* parent, register_t retval)
{
	/* Copy the entire context over */
	memcpy(&t->md_ctx, &parent->md_ctx, sizeof(t->md_ctx));

	/* Restore the thread's own page directory */
	t->md_ctx.cr3 = KVTOP((addr_t)t->md_pagedir);

	/*
	 * Temporarily disable the thread's interrupts; it's only possible to clone a
	 * thread by using a syscall and we need to restore the part of the context
	 * as ABI requirements dictate - thus the stack may not change and we'll
	 * disable interrupts to ensure it will not.
	 */
	t->md_ctx.eflags &= ~EFLAGS_IF;

	/* Do not inherit breakpoints */
	t->md_ctx.dr[7] = DR7_LE | DR7_GE;

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
	void* kstack_src = vm_map_kernel((addr_t)parent->md_kstack, KERNEL_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ);
	void* kstack_dst = vm_map_kernel((addr_t)t     ->md_kstack, KERNEL_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	memcpy(kstack_dst, kstack_src, KERNEL_STACK_SIZE);
	vm_unmap_kernel((addr_t)kstack_dst, KERNEL_STACK_SIZE / PAGE_SIZE);
	vm_unmap_kernel((addr_t)kstack_src, KERNEL_STACK_SIZE / PAGE_SIZE);

	/* Handle return value */
	t->md_ctx.cs = GDT_SEL_KERNEL_CODE;
	t->md_ctx.eip = (addr_t)&clone_return;
	t->md_ctx.eax = retval;
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
