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

static void
thread_code()
{
	while (1) {
#if 0
		uint32_t ctx = 0;
		kprintf("hi, %x ", ctx);
#endif
		/* syscall test */
		__asm(
			"movl	$0x1, %eax\n"
			"movl	$0x2, %ebx\n"
			"movl	$0x3, %ecx\n"
			"movl	$0x4, %edx\n"
			"movl	$0x5, %esi\n"
			"movl	$0x6, %edi\n"
			"int $0x30\n");

		/* force a switch! */
		__asm("int $0x90");
	}
}

int
md_thread_init(thread_t thread)
{
	/* Note that this function relies on thread->md being zero-filled before calling */
	struct MD_THREAD* md = (struct MD_THREAD*)thread->md;

	/* Create a pagedirectory and map the kernel pages in there */
	md->pagedir = kmalloc(PAGE_SIZE);
	memset(md->pagedir, 0, PAGE_SIZE);
	vm_map_kernel_addr(md->pagedir);

	/* XXX */
	char* buf = kmalloc(PAGE_SIZE);
	memcpy(buf, &thread_code, 4096);

	/* Allocate stacks: one for the thread and one for the kernel */
	md->stack  = kmalloc(THREAD_STACK_SIZE);
	md->kstack = kmalloc(KERNEL_STACK_SIZE);

	/* XXX Debugging! */
	md->ctx.eax = 0x12345679;
	md->ctx.ebx = 0x0f00c0de;
	md->ctx.ecx = 0xdeadbabe;
	md->ctx.edx = 0xcafeb00b;
	md->ctx.esi = 0xfaabbeef;
	md->ctx.edi = 0x87654321;
	md->ctx.esp  = (addr_t)md->stack  + THREAD_STACK_SIZE;
	md->ctx.esp0 = (addr_t)md->kstack + KERNEL_STACK_SIZE;
	md->ctx.eip = (addr_t)buf;

	/* Perform adequate mapping for the stack / code */
	vm_map_pagedir(md->pagedir, (addr_t)md->stack,  THREAD_STACK_SIZE / PAGE_SIZE, 1);
	vm_map_pagedir(md->pagedir, (addr_t)md->kstack, KERNEL_STACK_SIZE / PAGE_SIZE, 0);
	vm_map_pagedir(md->pagedir, (addr_t)buf, 1, 1);

	/* Fill out the thread's context */ 
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
