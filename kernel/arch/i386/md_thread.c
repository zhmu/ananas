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
#include <ananas/vm.h>
#include "options.h"

extern struct TSS kernel_tss;
void clone_return();
void userland_trampoline();
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

	/* Fill our the %esp and %cr3 fields */
	t->md_esp = (addr_t)USERLAND_STACK_ADDR + THREAD_STACK_SIZE;
	t->md_cr3 = KVTOP((addr_t)t->md_pagedir);
	t->md_esp0 = (addr_t)KERNEL_STACK_ADDR + KERNEL_STACK_SIZE - 4;

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
	t->md_eip = (addr_t)&userland_trampoline;

	return md_thread_setup(t);
}

static void
md_thread_push(thread_t* t, uint32_t value)
{
	*(uint32_t*)t->md_esp = value;
	t->md_esp -= sizeof(value);
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	/* Set up the environment */
	t->md_eip = (addr_t)kfunc;
	t->md_cr3 = KVTOP((addr_t)kernel_pd);

	/*
	 * Kernel threads share the kernel pagemap and thus need to map the kernel
	 * stack. We do not differentiate between kernel and userland stacks as
	 * no kernelthread ever runs userland code.
	 */
	t->md_kstack_ptr = kmalloc(KERNEL_STACK_SIZE);
	t->md_esp = (addr_t)t->md_kstack_ptr + KERNEL_STACK_SIZE - 4;
	md_thread_push(t, (uint32_t)arg);
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
	KASSERT((new->flags & THREAD_FLAG_ZOMBIE) == 0, "cannot switch to a zombie thread");

	/* Activate the corresponding kernel stack in the TSS */
	struct TSS* tss = (struct TSS*)PCPU_GET(tss);
	tss->esp0 = new->md_esp0;

	/* Set debug registers */
	DR_SET(0, new->md_dr[0]);
	DR_SET(1, new->md_dr[1]);
	DR_SET(2, new->md_dr[2]);
	DR_SET(3, new->md_dr[3]);
	DR_SET(7, new->md_dr[7]);

#if 0
	kprintf("old: eip=%p esp=%p; new: eip=%p esp=%p\n",
		 old->md_eip, old->md_esp,
		 new->md_eip, new->md_esp);
	//__asm("xchgw %bx,%bx");
#endif

	/*
	 * Switch to the new thread; as this will only happen in kernel -> kernel
	 * contexts and even then, between a frame, it is enough to just
	 * switch the stack and mark all registers are being clobbered by assigning
	 * them to dummy outputs.
	 *
	 * Note that we can't force the compiler to reload %ebp this way, so we'll
	 * just save it ourselves.
	 */
	register_t ebx, ecx, edx, esi, edi;
	__asm __volatile(
		"pushfl\n"
		"pushl %%ebp\n"
		"movl %%esp,%0\n" /* store old %esp */
		"movl %1, %%esp\n" /* write new %esp */
		"movl $1f, %2\n" /* write next %eip for old thread */
		"movl %9, %%cr3\n"
		"pushl %8\n" /* load next %eip for new thread */
		"ret\n"
	"1:\n" /* we are back */
		"popl %%ebp\n"
		"popfl\n"
	: /* out */
		"=m" (old->md_esp), "=m" (new->md_esp),
		"=m" (old->md_eip),
		/* clobber */
		"=b" (ebx), "=c" (ecx), "=d" (edx), "=S" (esi), "=D" (edi)
	: /* in */
		"m" (new->md_eip), "r" (new->md_cr3)
	);
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
	void* kstack_src = vm_map_kernel((addr_t)parent->md_kstack, KERNEL_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ);
	void* kstack_dst = vm_map_kernel((addr_t)t     ->md_kstack, KERNEL_STACK_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
	memcpy(kstack_dst, kstack_src, KERNEL_STACK_SIZE);
	vm_unmap_kernel((addr_t)kstack_dst, KERNEL_STACK_SIZE / PAGE_SIZE);
	vm_unmap_kernel((addr_t)kstack_src, KERNEL_STACK_SIZE / PAGE_SIZE);

	/*
	 * Handle returning to the new thread; this just cancels the system call
	 * and overrides the return value.
	 */
	t->md_esp = parent->md_esp;
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
