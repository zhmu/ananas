#include "machine/thread.h"
#include "machine/vm.h"
#include "param.h"
#include "thread.h"
#include "lib.h"
#include "mm.h"
#include "vm.h"

extern struct TSS kernel_tss;

void vm_map_pagedir(uint64_t* pml4, addr_t addr, size_t num_pages, uint32_t user);

int
md_thread_init(thread_t thread)
{
	/* Note that this function relies on thread->md being zero-filled before calling */
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;

	/* Create a pagedirectory and map the kernel pages in there */
	md->pml4 = kmalloc(PAGE_SIZE);
	memset(md->pml4, 0, PAGE_SIZE);
	vm_map_kernel_addr(md->pml4);

	/* Allocate stacks: one for the thread and one for the kernel */
	md->stack  = kmalloc(THREAD_STACK_SIZE);
	md->kstack = kmalloc(KERNEL_STACK_SIZE);

	/* Perform adequate mapping for the stack / code */
	vm_map_pagedir(md->pml4, (addr_t)md->stack,  THREAD_STACK_SIZE / PAGE_SIZE, 1);
	vm_map_pagedir(md->pml4, (addr_t)md->kstack, KERNEL_STACK_SIZE / PAGE_SIZE, 0);

	/* Set up the context  */
	md->ctx.sf.sf_rax = 0x123456789abcdef;
	md->ctx.sf.sf_rbx = 0xdeadf00dbabef00;

	md->ctx.sf.sf_rsp = (addr_t)md->stack  + THREAD_STACK_SIZE - 8 /* XXX */;
	md->ctx.sf.sf_sp  = (addr_t)md->kstack + KERNEL_STACK_SIZE - 8 /* XXX */;
	md->ctx.sf.sf_cs = GDT_SEL_USER_CODE + SEG_DPL_USER;
	md->ctx.sf.sf_ss = GDT_SEL_KERNEL_DATA;
	md->ctx.sf.sf_rflags = 0x200 /* RFLAGS_IF */;
	md->ctx.pml4 = md->pml4;

	return 1;
}

size_t
md_thread_get_privdata_length()
{
	return sizeof(struct MD_THREAD);
}

void
md_thread_destroy(thread_t thread)
{
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;

	kfree(md->pml4);
	kfree(md->stack);
	kfree(md->kstack);
}

void
md_thread_switch(thread_t new, thread_t old)
{
	struct MD_THREAD* md_new = (struct MD_THREAD*)new->md;
	struct CONTEXT* ctx_new = (struct CONTEXT*)&md_new->ctx;

	/*
	 * Activate this context as the current CPU context. XXX lock
	 */
	__asm(
		"movl	%%eax, %%gs:0\n"
	: : "a" (ctx_new));

	if (old != NULL) {
		struct MD_THREAD* md_old = (struct MD_THREAD*)old->md;
		struct CONTEXT* ctx_old = (struct CONTEXT*)&md_old->ctx;

		/* Activate the corresponding kernel stack in the TSS */
		//ctx_old->esp0 = tss->esp0;
	}

	/* Activate the corresponding kernel stack in the TSS */
	kernel_tss.rsp0 = ctx_new->sf.sf_sp;

	/* Go! */
	md_restore_ctx(ctx_new);
}

/* vim:set ts=2 sw=2: */
