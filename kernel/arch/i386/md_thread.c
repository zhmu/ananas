#include <sys/types.h>
#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/macro.h>
#include <machine/smp.h>
#include <machine/realmode.h>
#include <sys/lib.h>
#include <sys/mm.h>
#include <sys/pcpu.h>
#include <sys/thread.h>
#include <sys/vm.h>
#include "options.h"

extern struct TSS kernel_tss;

int
md_thread_init(thread_t t)
{
	/* Create a pagedirectory and map the kernel pages in there */
	t->md_pagedir = kmalloc(PAGE_SIZE);
	memset(t->md_pagedir, 0, PAGE_SIZE);
	vm_map_kernel_addr(t->md_pagedir);

	/* Allocate stacks: one for the thread and one for the kernel */
	t->md_stack  = kmalloc(THREAD_STACK_SIZE);
	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);

	/* Perform adequate mapping for the stack / code */
	vm_map_pagedir(t->md_pagedir, (addr_t)t->md_stack,  THREAD_STACK_SIZE / PAGE_SIZE, 1);
	vm_map_pagedir(t->md_pagedir, (addr_t)t->md_kstack, KERNEL_STACK_SIZE / PAGE_SIZE, 0);

#ifdef SMP	
	/*
	 * Grr - for some odd reason, the GDT had to be subject to paging. This means
	 * we have to insert a suitable mapping for every CPU... :-/
	 */
	uint32_t i;
	for (i = 0; i < get_num_cpus(); i++) {
		struct IA32_CPU* cpu = get_cpu_struct(i);
		vm_map_pagedir(md->pagedir, (addr_t)cpu->gdt, 1 /* XXX */, 0);
	}
#endif

	/* Fill out the thread's registers - anything not here will be zero */ 
	t->md_ctx.esp  = (addr_t)t->md_stack  + THREAD_STACK_SIZE;
	t->md_ctx.esp0 = (addr_t)t->md_kstack + KERNEL_STACK_SIZE;
	t->md_ctx.cs = GDT_SEL_USER_CODE + SEG_DPL_USER;
	t->md_ctx.ds = GDT_SEL_USER_DATA;
	t->md_ctx.es = GDT_SEL_USER_DATA;
	t->md_ctx.ss = GDT_SEL_USER_DATA + SEG_DPL_USER;
	t->md_ctx.cr3 = (addr_t)t->md_pagedir;
	t->md_ctx.eflags = EFLAGS_IF;

	t->next_mapping = 1048576;
	return 1;
}

void
md_thread_destroy(thread_t t)
{
	kfree(t->md_pagedir);
	kfree(t->md_stack);
	kfree(t->md_kstack);
}

void
md_thread_switch(thread_t new, thread_t old)
{
	struct CONTEXT* ctx_new = (struct CONTEXT*)&new->md_ctx;

	/*
	 * Activate this context as the current CPU context. XXX lock
	 */
	__asm(
		"movw %%bx, %%fs\n"
		"movl	%%eax, %%fs:0\n"
	: : "a" (ctx_new), "b" (GDT_SEL_KERNEL_PCPU));

	/* Fetch kernel TSS */
	struct TSS* tss;
	__asm(
		"movl	%%fs:8, %0\n"
	: "=r" (tss));

	/* Activate the corresponding kernel stack in the TSS */
	tss->esp0 = ctx_new->esp0;

	/* Go! */
	md_restore_ctx(ctx_new);
}

void*
md_map_thread_memory(thread_t thread, void* ptr, size_t length, int write)
{
	KASSERT(length <= PAGE_SIZE, "no support for >PAGE_SIZE mappings yet!");

	addr_t addr = (addr_t)ptr & ~(PAGE_SIZE - 1);
	addr_t phys = vm_get_phys(thread->md_pagedir, addr, write);
	if (phys == 0)
		return NULL;

	addr_t virt = TEMP_USERLAND_ADDR + PCPU_GET(cpuid) * TEMP_USERLAND_SIZE;
	vm_mapto(virt, phys, 2 /* XXX */);
	return (void*)virt + ((addr_t)ptr % PAGE_SIZE);
}

void*
md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags)
{
	int num_pages = length / PAGE_SIZE;
	if (length % PAGE_SIZE > 0)
		num_pages++;
	/* XXX cannot specify flags yet */
	vm_mapto_pagedir(thread->md_pagedir, (addr_t)to, (addr_t)from, num_pages, 1);
	return to;
}

int
md_thread_unmap(thread_t thread, void* addr, size_t length)
{
	int num_pages = length / PAGE_SIZE;
	if (length % PAGE_SIZE > 0)
		num_pages++;
	vm_unmap_pagedir(thread->md_pagedir, (addr_t)addr, num_pages);
	return 0;
}

void
md_thread_set_entrypoint(thread_t thread, addr_t entry)
{
	thread->md_ctx.eip = entry;
}

/* vim:set ts=2 sw=2: */
