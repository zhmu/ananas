#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <ananas/thread.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/syscall.h>
#include <ananas/vm.h>

extern struct TSS kernel_tss;
void md_idle_thread();
void clone_return();

errorcode_t
md_thread_init(thread_t* t)
{
	/* Create a pagedirectory and map the kernel pages in there */
	t->md_pml4 = kmalloc(PAGE_SIZE);
	memset(t->md_pml4, 0, PAGE_SIZE);
	vm_map_kernel_addr(t->md_pml4);

	/* Allocate stacks: one for the thread and one for the kernel */
	t->md_stack  = kmalloc(THREAD_STACK_SIZE);
	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);

	/* Perform adequate mapping for the stack / code */
	vm_mapto_pagedir(t->md_pml4, USERLAND_STACK_ADDR, (addr_t)t->md_stack,  THREAD_STACK_SIZE / PAGE_SIZE, 1);
	vm_map_pagedir(t->md_pml4, (addr_t)t->md_kstack, KERNEL_STACK_SIZE / PAGE_SIZE, 0);

	/* Set up the context  */
	t->md_ctx.sf.sf_rax = 0x123456789abcdef;
	t->md_ctx.sf.sf_rbx = 0xdeadf00dbabef00;

	t->md_ctx.sf.sf_rsp = (addr_t)USERLAND_STACK_ADDR + THREAD_STACK_SIZE;
	t->md_ctx.sf.sf_sp  = (addr_t)t->md_kstack + KERNEL_STACK_SIZE;

	t->md_ctx.sf.sf_cs = GDT_SEL_USER_CODE + SEG_DPL_USER;
	t->md_ctx.sf.sf_ss = GDT_SEL_USER_DATA + SEG_DPL_USER;
	t->md_ctx.sf.sf_rflags = 0x200 /* RFLAGS_IF */;
	t->md_ctx.pml4 = (addr_t)t->md_pml4;

	/* initialize FPU state similar to what finit would do */
	t->md_ctx.fpu.fcw = 0x37f;
	t->md_ctx.fpu.ftw = 0xffff;

	t->t_next_mapping = 1048576;
	return ANANAS_ERROR_OK;
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	errorcode_t err = md_thread_init(t);
	ANANAS_ERROR_RETURN(err);

	t->md_ctx.sf.sf_ss = GDT_SEL_KERNEL_DATA;
	t->md_ctx.sf.sf_cs = GDT_SEL_KERNEL_CODE;
	t->md_ctx.sf.sf_rip = (addr_t)kfunc;
	t->md_ctx.sf.sf_rdi = (addr_t)arg;

	/*
	 * Kernel threads only have a single stack; they cannot use the userland
	 * stack because it is not mapped.
	 */
	t->md_ctx.sf.sf_rsp = t->md_ctx.sf.sf_sp;
	return ANANAS_ERROR_OK;
}

void
md_thread_free(thread_t* t)
{
	kfree(t->md_pml4);
	kfree(t->md_stack);
	kfree(t->md_kstack);
}

void
md_thread_switch(thread_t* new, thread_t* old)
{
	struct CONTEXT* ctx_new = (struct CONTEXT*)&new->md_ctx;

	/*
	 * Activate this context as the current CPU context. XXX lock
	 */
	PCPU_SET(context, ctx_new);
	PCPU_SET(kernel_rsp, ctx_new->sf.sf_sp);

	/* Activate the corresponding kernel stack in the TSS */
	kernel_tss.rsp0 = ctx_new->sf.sf_sp;

	/* Go! */
	md_restore_ctx(ctx_new);
}

void*
md_map_thread_memory(thread_t* thread, void* ptr, size_t length, int write)
{
	return ptr;
}

int
md_thread_is_mapped(thread_t* thread, addr_t virt, int flags, addr_t* va)
{
	return vm_get_phys(thread->md_pml4, virt, flags, va);
}

void*
md_thread_map(thread_t* thread, void* to, void* from, size_t length, int flags)
{
	int num_pages = length / PAGE_SIZE;
	if (length % PAGE_SIZE > 0)
		num_pages++;
	/* XXX cannot specify flags yet */
	vm_mapto_pagedir(thread->md_pml4, (addr_t)to, (addr_t)from, num_pages, 1);
	return to;
}

errorcode_t
md_thread_unmap(thread_t* thread, addr_t addr, size_t length)
{
	int num_pages = length / PAGE_SIZE;
	if (length % PAGE_SIZE > 0)
		num_pages++;
#if 0
	vm_unmap_pagedir(thread->md_pml4, addr, num_pages);
#endif
	panic("md_thread_unmap() todo");
	return ANANAS_ERROR_OK;
}

void
md_thread_set_entrypoint(thread_t* thread, addr_t entry)
{
	thread->md_ctx.sf.sf_rip = entry;
}

void
md_thread_set_argument(thread_t* thread, addr_t arg)
{
	thread->md_ctx.sf.sf_rdi = arg;
}

void
md_thread_clone(struct THREAD* t, struct THREAD* parent, register_t retval)
{
	/* Copy the entire context over */
	memcpy(&t->md_ctx, &parent->md_ctx, sizeof(t->md_ctx));

	/*
	 * A kernel stack is not subject to paging; this is unavoidable as we need it
	 * during a context switch, so it must reside in the kernel address space.
	 *
	 * This means we'll have to reinitialize a kernel stack - the clone_return
	 * code will perform the adequate magic.
	 */
	t->md_ctx.sf.sf_sp = (addr_t)t->md_kstack + KERNEL_STACK_SIZE;
	t->md_ctx.pml4 = (addr_t)t->md_pml4;

	/*
	 * Because our interrupt code uses the stack to temporary stash variables, we
	 * must protect our stackframe. The value is directly used in interrupts.S,
	 * so be careful when changing it.
	 */
	t->md_ctx.sf.sf_sp -= sizeof(struct SYSCALL_ARGS) + 80;

	/*
	 * Copy stack content; we copy the kernel stack over
	 * because we can obtain the stackframe from it, which
	 * allows us to return to the intended caller.
	 */
	memcpy(t->md_stack,  parent->md_stack, THREAD_STACK_SIZE);
	memcpy(t->md_kstack, parent->md_kstack, KERNEL_STACK_SIZE);

	/* Handle return value */
	t->md_ctx.sf.sf_cs = GDT_SEL_KERNEL_CODE;
	t->md_ctx.sf.sf_rip = (addr_t)&clone_return;
	t->md_ctx.sf.sf_rax = retval;
}

/* vim:set ts=2 sw=2: */
