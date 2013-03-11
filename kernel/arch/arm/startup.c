#include <ananas/types.h>
#include <ananas/arm/mv-regs.h>
#include <ananas/arm/param.h>
#include <ananas/arm/vm.h>
#include <ananas/debug-console.h>
#include <ananas/vm.h>
#include <ananas/pcpu.h>
#include <ananas/lib.h>
#include "options.h"

struct PCPU cpu_pcpu;

/* Bootinfo as supplied by the loader, or NULL */
struct BOOTINFO* bootinfo = NULL;
extern void *__entry, *__end;

static void
foo()
{	
	*(int*)0 = 1;
}

void
md_startup()
{
	/*
	 * These are our first few steps in the world of boot - first of all, we
	 * need to get the memory mappings right - this code is running from
	 * unmapped memory addresses and there is no MMU enabled.
	 *
	 * Prepare pagetables; we need to map the lower kernel addresses (where
	 * we are now) and the upper kernel addresses (where we want to execute
	 * from) - note that we must map the initial table to TT_ALIGN-sized boundary
	 * to avoid problems.
	 *
	 * We reserve memory so that kernel memory can always be mapped without
	 * needing to allocate the coarse entry.
	 */
	addr_t address = (addr_t)&__end - KERNBASE;
	address = (address | (VM_TT_ALIGN - 1)) + 1;

	/* Create the translation table; i.e. the page directory */
	uint32_t* tt = (uint32_t*)address;
	memset(tt, 0, VM_TT_SIZE);
	address += VM_TT_SIZE;

	/* Hook up the initial kernel second-level structure mappings */
	for (int i = 0; i < VM_TT_KERNEL_NUM; i++) {
		tt[VM_TT_KERNEL_START + i] = address | VM_TT_TYPE_COARSE; /* XXX domain=0 */
		memset((void*)address, 0, VM_L2_TABLE_SIZE);
		address += VM_L2_TABLE_SIZE;
	}

	/*
	 * Number of extra pages we need - currently, we use:
	 * - 1 for the vector page
	 * - 2 per CPU mode stack (svc/abt/und/irq/fiq)
	 */
#define NUM_CPU_MODES 5
#define CPU_MODE_STACK_SIZE 2
#define EXTRA_PAGES ((1 + (NUM_CPU_MODES * CPU_MODE_STACK_SIZE)) * PAGE_SIZE)

	/*
	 * Create the kernel table entry mappings - we map the page tables themselves too
	 * so that we can always create kernel memory mappings.
	 */
	for (addr_t n = (addr_t)&__entry; n < (addr_t)(address + EXTRA_PAGES + KERNBASE); n += PAGE_SIZE) {
		uint32_t* l1 = (uint32_t*)&tt[n >> 20]; /* note that typeof(tt) is uint32_t, so no << 2 is needed */
		uint32_t* l2 = (uint32_t*)((*l1 & 0xfffffc00) + (((n & 0xff000) >> 12) << 2));
		*l2 = ((n - KERNBASE) & 0xfffff000) | VM_COARSE_SMALL_AP(VM_AP_KRW_URO) | VM_COARSE_TYPE_SMALL;
	}

	/*
	 * Establish an identity mapping for the current kernel addresses; this is necessary
	 * so that we can make the switch to MMU-world without losing our current code.
	 */
	for (addr_t n = (addr_t)&__entry; n < (addr_t)&__end; n += PAGE_SIZE) {
		tt[KVTOP(n) >> 20] = tt[n >> 20];
	}

	/* Time to throw... the switch: the pagetables need to be set */
	__asm __volatile(
		"mcr p15, 0, %0, c2, c0, 0\n"		/* set translation table base */
		"mcr p15, 0, %1, c8, c7, 0\n"		/* invalidate entire TLB */
		"mov r1, #1\n"									/* domain stuff */
		"mcr p15, 0, r1, c3, c0, 0\n"

		"mov r1, #0\n"									/* control reg */
		"mcr p15, 0, r1, c2, c0, 2\n"

		"mrc p15, 0, r1, c1, c0, 0\n"
		"orr r1, r1, #1\n"							/* enable MMU */
		"mcr p15, 0, r1, c1, c0, 0\n"
		/* flush instruction cache */
		"nop\n"
		"nop\n"
		"nop\n"
		/* perform a dummy r16 read */
		"mrc p15, 0, r1, c2, c0, 0\n"
		"mov r1, r1\n"
		/* alter our instruction, frame and stack pointer */
		"add	sp, sp, %2\n"
		"add	fp, fp, %2\n"
		"add	pc, pc, %2\n"
		"nop\n"
	: : "r" (tt), "r" (0), "r" (KERNBASE) : "r1");

	/*
	 * Initialize kernel bss to zero; this won't be done by any loader because the
	 * entries aren't part of the kernel image. We do it here because now the
	 * mappings are valid.
	 */
	extern void* __bss_begin;
	extern void* __bss_end;
	memset((void*)&__bss_begin, 0, &__bss_end - &__bss_begin);

	/*
	 * Hand the TT we created to the VM - it is safe to do so now as we have the
	 * mappings to do so.
	 */
	vm_kernel_tt = (uint32_t*)PTOKV((addr_t)tt);

#ifdef OPTION_DEBUG_CONSOLE
	debugcon_init();
#endif

	/*
	 * Initialize a page for our vectors; we'll relocate this to 0xffff0000 so
	 * that we can still trap NULL pointers.
	 */
	md_mapto(vm_kernel_tt, 0xFFFF0000, address, 1, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_EXECUTE);
	address += PAGE_SIZE;

	/* Copy the vectors to it */
	extern void* _vectors_page;
	memcpy((void*)0xFFFF0000, (void*)&_vectors_page, PAGE_SIZE);

	/* Move the exception vectors to 0xFFFFxxxx */
	__asm(
		"mrc p15, 0, r1, c1, c0, 0\n"
		"orr r1, r1, #0x2000\n"							/* V (bit 13): vector base */
		"mcr p15, 0, r1, c1, c0, 0\n"
	);

	/*
	 * Assign stacks for all CPU modes; the ARM will do an implicit stack switch
	 * when the mode is switched around, so we need to switch to each mode and
	 * set up the stack.
	 */
	__asm(
		/* Switch to abort mode */
		"mrs r1, cpsr\n"					/* r1 = current cpsr */
		"bic r4, r1, #0x1f\n"			/* clear current mode */
		"orr r2, r4, #0x17\n"			/* -> abort mode */
		"msr CPSR_c, r2\n"				/* activate new mode */
		/* Activate our stack and update it */
		"add	%0, %0, %1\n"
		"mov	sp, %0\n"
		/* Switch to undefined mode */
		"orr r2, r4, #0x1b\n"			/* -> abort mode */
		"msr CPSR_c, r2\n"				/* activate new mode */
		/* Activate our stack and update it */
		"add	%0, %0, %1\n"
		"mov	sp, %0\n"
		/* Switch to IRQ mode */
		"orr r2, r4, #0x12\n"			/* -> irq mode */
		"msr CPSR_c, r2\n"				/* activate new mode */
		/* Activate our stack and update it */
		"add	%0, %0, %1\n"
		"mov	sp, %0\n"
		/* Switch to FIQ mode */
		"orr r2, r4, #0x11\n"			/* -> irq mode */
		"msr CPSR_c, r2\n"				/* activate new mode */
		/* Activate our stack and update it */
		"add	%0, %0, %1\n"
		"mov	sp, %0\n"
		/* Switch back to supervisor mode */
		"msr CPSR_c, r1\n"				/* activate new mode */
	: : "r" (address + KERNBASE), "r" (CPU_MODE_STACK_SIZE * PAGE_SIZE) : "1", "2", "4");

	/* Throw away the identity mappings; these are not needed anymore */
	for (addr_t n = (addr_t)&__entry; n < (addr_t)&__end; n += PAGE_SIZE) {
		tt[KVTOP(n) >> 20] = 0;
	}

	kprintf("Hello world");

	foo();

	for(;;) {
		foo();
	}
}

/* vim:set ts=2 sw=2: */
