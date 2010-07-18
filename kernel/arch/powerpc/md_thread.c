#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/mmu.h>
#include <sys/thread.h>
#include <sys/lib.h>
#include <sys/mm.h>
#include <sys/pcpu.h>
#include <sys/vm.h>

extern struct TSS kernel_tss;
void md_idle_thread();

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

	/* Allocate a stack for the thread */
	t->md_stack  = kmalloc(THREAD_STACK_SIZE);
	md_thread_map(t, t->md_stack, t->md_stack, THREAD_STACK_SIZE, 0);
	t->md_ctx.sf.sf_reg[1] = (addr_t)t->md_stack + THREAD_STACK_SIZE;

	/*
	 * XXX Figure out why this is necessary; why do our threads try to access
	 * things outside of the stack frame ?
	 */
	t->md_ctx.sf.sf_reg[1] -= 64;

	/* Establish initial context */
	t->md_ctx.sf.sf_srr1 = MSR_DR | MSR_IR | MSR_RI | MSR_PR;
	/* XXX Until FPU support is properly implemented */
	t->md_ctx.sf.sf_srr1 |= MSR_FP;

	t->next_mapping = 1048576;
	return 1;
}

void
md_thread_destroy(thread_t t)
{
	panic("md_thread_destroy(): not implemented");
}

void*
md_map_thread_memory(thread_t thread, void* ptr, size_t length, int write)
{
	panic("md_map_thread_memory(): not implemented");
	return NULL;
}

void*
md_thread_map(thread_t thread, void* to, void* from, size_t length, int flags)
{
	KASSERT((((addr_t)to % PAGE_SIZE) == 0), "to address not page-aligned");
	KASSERT((((addr_t)from % PAGE_SIZE) == 0), "from address not page-aligned");

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
}

void
md_thread_set_entrypoint(thread_t thread, addr_t entry)
{
	thread->md_ctx.sf.sf_nia = entry;
}

/* vim:set ts=2 sw=2: */
