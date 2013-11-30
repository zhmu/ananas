#include <machine/param.h>
#include <machine/thread.h>
#include <machine/vm.h>
#include <machine/mmu.h>
#include <machine/macro.h>
#include <ananas/thread.h>
#include <ananas/lib.h>
#include <ananas/error.h>
#include <ananas/mm.h>
#include <ananas/pcpu.h>
#include <ananas/vm.h>

/* Segment register used for temporary maps */
#define THREAD_MAP_SR (THREAD_MAP_ADDR & 0xf0000000)

void
md_idle_thread()
{
	/* XXX this needs some work */
	while(1);
}

errorcode_t
md_thread_init(thread_t* t)
{
	errorcode_t err;

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
	struct THREAD_MAPPING* tm;
	err = thread_mapto(t, (addr_t)stack_addr, (addr_t)t->md_stack, THREAD_STACK_SIZE, 0, &tm);
	if (err != ANANAS_ERROR_NONE) {
		kfree(t->md_stack);
		/* XXX remove kernel mmu mappings */
		return err;
	}
	t->md_ctx.sf.sf_reg[1] = stack_addr + THREAD_STACK_SIZE;

	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);
	memset(t->md_kstack, 0, KERNEL_STACK_SIZE);
	t->md_kstack += KERNEL_STACK_SIZE - 16;

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
	t->t_next_mapping = 0x4000000;
	return ANANAS_ERROR_OK;
}

errorcode_t
md_kthread_init(thread_t* t, kthread_func_t kfunc, void* arg)
{
	/* Duplicate the kernel's memory mappings */
	for (int i = 0; i < PPC_NUM_SREGS; i++)
		t->md_ctx.sf.sf_sr[i] = bsp_sf.sf_sr[i];

	/* Establish context */
	t->md_ctx.sf.sf_srr1 = MSR_DR | MSR_IR | MSR_RI | MSR_EE;

	/* Allocate a stack; we'll only need a kernel stack */
	t->md_kstack = kmalloc(KERNEL_STACK_SIZE);
	t->md_ctx.sf.sf_reg[1] = (addr_t)t->md_kstack + THREAD_STACK_SIZE;

	/* Setup function to call with argument */
	t->md_ctx.sf.sf_srr0 = (addr_t)kfunc;
	t->md_ctx.sf.sf_reg[3] = (addr_t)arg;
	return ANANAS_ERROR_OK;
}

void
md_thread_free(thread_t* t)
{
	/* XXX we should unmap everything the thread has mapped here */
	kfree(t->md_stack);
}

void*
md_map_thread_memory(thread_t* thread, void* ptr, size_t length, int write)
{
	/*
	 * Mapping thread memory works by obtaining the VSID for the given user pointer segment,
	 * and updating out kernel's mapping VSID so that it corresponds with the user one; this
	 * allows us to just fetch the value without any issues.
	 *
	 * XXX We should deal with data storage problems in the exception handler; they must abort
	 *     the system call.
	 */
	int sr = (addr_t)ptr >> 28;
	uint32_t vsid = thread->md_ctx.sf.sf_sr[sr];
	if (vsid == INVALID_VSID)
		return NULL;

	struct STACKFRAME* cpu_sf = PCPU_GET(context);
	cpu_sf->sf_sr[THREAD_MAP_SR >> 28] = vsid;
	mtsrin(THREAD_MAP_SR, vsid);

	return (void*)(((addr_t)ptr & 0x0fffffff) | THREAD_MAP_SR);
}

void*
md_thread_map(thread_t* thread, void* to, void* from, size_t length, int flags)
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

errorcode_t
md_thread_unmap(thread_t* thread, addr_t addr, size_t length)
{
	panic("md_thread_unmap(): not implemented");
	return ANANAS_ERROR_OK;
}

void
md_thread_set_entrypoint(thread_t* thread, addr_t entry)
{
	thread->md_ctx.sf.sf_srr0 = entry;
}

void
md_thread_set_argument(thread_t* thread, addr_t arg)
{
	thread->md_ctx.sf.sf_reg[3] = arg;
}

int
md_thread_is_mapped(thread_t* thread, addr_t virt, int flags, addr_t* va)
{
	return mmu_resolve_mapping(&thread->md_ctx.sf, virt, flags, va);
}

void
md_thread_clone(struct THREAD* t, struct THREAD* parent, register_t retval)
{
	panic("md_thread_clone");
}

/* vim:set ts=2 sw=2: */
