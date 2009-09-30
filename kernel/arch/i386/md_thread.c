#include "i386/thread.h"
#include "i386/vm.h"
#include "i386/macro.h"
#include "lib.h"
#include "mm.h"
#include "param.h"
#include "thread.h"

void md_restore_ctx(struct CONTEXT* ctx);
void vm_map_pagedir(uint32_t* pagedir, addr_t addr, size_t num_pages, int user);

extern struct TSS kernel_tss;

int
md_thread_init(thread_t thread)
{
	/* Note that this function relies on thread->md being zero-filled before calling */
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;

	/* Create a pagedirectory and map the kernel pages in there */
	md->pagedir = kmalloc(PAGE_SIZE);
	memset(md->pagedir, 0, PAGE_SIZE);
	vm_map_kernel_addr(md->pagedir);

	/* Allocate stacks: one for the thread and one for the kernel */
	md->stack  = kmalloc(THREAD_STACK_SIZE);
	md->kstack = kmalloc(KERNEL_STACK_SIZE);

	/* Perform adequate mapping for the stack / code */
	vm_map_pagedir(md->pagedir, (addr_t)md->stack,  THREAD_STACK_SIZE / PAGE_SIZE, 1);
	vm_map_pagedir(md->pagedir, (addr_t)md->kstack, KERNEL_STACK_SIZE / PAGE_SIZE, 0);

	/* Fill out the thread's registers - anything not here will be zero */ 
	md->ctx.esp  = (addr_t)md->stack  + THREAD_STACK_SIZE;
	md->ctx.esp0 = (addr_t)md->kstack + KERNEL_STACK_SIZE;
	md->ctx.cs = GDT_SEL_USER_CODE + SEG_DPL_USER;
	md->ctx.ds = GDT_SEL_USER_DATA;
	md->ctx.es = GDT_SEL_USER_DATA;
	md->ctx.ss = GDT_SEL_USER_DATA + SEG_DPL_USER;
	md->ctx.cr3 = (addr_t)md->pagedir;

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

	kfree(md->pagedir);
	kfree(md->stack);
	kfree(md->kstack);
}

void
md_thread_switch(thread_t thread)
{
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;
	struct CONTEXT* ctx = (struct CONTEXT*)&md->ctx;

	/*
	 * Activate this context as the current CPU context. XXX lock
	 */
	__asm(
		"movw %%bx, %%fs\n"
		"movl	%%eax, %%fs:0\n"
	: : "a" (ctx), "b" (GDT_SEL_KERNEL_PCPU));

	/* Activate the corresponding kernel stack in the TSS */
	kernel_tss.esp0 = ctx->esp0;

	/* Go! */
	md_restore_ctx(ctx);
}

/* vim:set ts=2 sw=2: */
