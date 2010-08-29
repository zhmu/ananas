#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/mmu.h>
#include <sys/thread.h>
#include <sys/lib.h>
#include <sys/mm.h>
#include <sys/pcpu.h>
#include <sys/vm.h>

void
md_idle_thread()
{
	/* XXX this needs some work */
	while(1);
}

#define THREAD_TEMP_SPACE 0xd0008000

int
md_thread_init(thread_t t)
{
	/*
	 * First of all, reset all segment register mappings; this will
	 * result in mmu_map() doing the right thing by allocating
	 * new segments as needed.
	 */
	for (int i = 0; i < PPC_NUM_SREGS; i++)
		t->md_ctx.sf.sf_sr[i] = INVALID_VSID;

	/* Establish a kernel mapping */
	mmu_map_kernel(&t->md_ctx.sf);

	/* Allocate a stack for the thread */
	uint32_t stack_addr = 0x8000000;
	t->md_stack  = kmalloc(THREAD_STACK_SIZE);
	memset(t->md_stack, 0, THREAD_STACK_SIZE);
	thread_mapto(t, stack_addr, t->md_stack, THREAD_STACK_SIZE, 0);
	t->md_ctx.sf.sf_reg[1] = stack_addr + THREAD_STACK_SIZE;

	/*
	 * Initially, the E500 ABI specifies that the stack frame
	 * must be 16 byte aligned, and that it must point to a single
	 * zero word.
	 */
	t->md_ctx.sf.sf_reg[1] -= 16;

	/* Establish initial context */
	t->md_ctx.sf.sf_srr1 = MSR_DR | MSR_IR | MSR_RI | MSR_PR | MSR_EE;
	/* XXX Until FPU support is properly implemented */
	t->md_ctx.sf.sf_srr1 |= MSR_FP;

	/* XXX We should use a more sensible address here */
	t->next_mapping = 0x4000000;
	return 1;
}

void
md_thread_destroy(thread_t t)
{
	/* XXX we should unmap everything the thread has mapped here */
	kfree(t->md_stack);
}

void*
md_map_thread_memory(thread_t thread, void* ptr, size_t length, int write)
{
	KASSERT(length <= PAGE_SIZE, "cannot map more than a page yet"); /* XXX */
//	kprintf("md_map_thread_memory(): t=%x, ptr=%p, length=%u\n", thread, ptr, length);

	addr_t addr = thread_find_mapping(thread, ptr);
	if (addr == 0)
		return NULL;
	uint32_t delta = (addr_t)ptr & (PAGE_SIZE - 1);

	/* yes this is cruel */
	vm_unmap(THREAD_TEMP_SPACE, 1);

	vm_mapto(THREAD_TEMP_SPACE, (addr_t)(addr - delta), 1);
	__asm __volatile("isync"); /* FIXME why is this needed? */
	return (void*)(THREAD_TEMP_SPACE + delta);
}

void*
md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags)
{
	KASSERT((((addr_t)to % PAGE_SIZE) == 0), "to address %p not page-aligned", to);
	KASSERT((((addr_t)from % PAGE_SIZE) == 0), "from address %p not page-aligned", from);

	int num_pages = length / PAGE_SIZE;
	if ((length % PAGE_SIZE) > 0)
		num_pages++;
	uint32_t virt = (uint32_t)to;
	uint32_t phys = (uint32_t)from;
	while (num_pages > 0) {
		mmu_map(&thread->md_ctx.sf, virt, phys);
		virt += PAGE_SIZE; phys += PAGE_SIZE;
		num_pages--;
	}
	return NULL;
}

int
md_thread_unmap(thread_t thread, void* addr, size_t length)
{
	panic("md_thread_unmap(): not implemented");
	return 0;
}

void
md_thread_setidle(thread_t thread)
{
	thread->md_ctx.sf.sf_srr0 = (addr_t)&md_idle_thread;
}

void
md_thread_set_entrypoint(thread_t thread, addr_t entry)
{
	thread->md_ctx.sf.sf_srr0 = entry;
}

/* vim:set ts=2 sw=2: */
