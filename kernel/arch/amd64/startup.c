#include <ananas/types.h>
#include <machine/param.h>
#include <machine/macro.h>
#include <machine/vm.h>
#include <machine/pcpu.h>
#include <machine/thread.h>
#include <ananas/x86/pic.h>
#include <ananas/x86/pit.h>
#include <ananas/x86/smap.h>
#include <ananas/handle.h>
#include <ananas/bootinfo.h>
#include <ananas/init.h>
#include <ananas/vm.h>
#include <ananas/pcpu.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <loader/module.h>

/* Pointer to the next available page */
void* avail;

/* Pointer to the page directory level 4 */
uint64_t* pml4;

/* Global Descriptor Table */
uint8_t gdt[GDT_NUM_ENTRIES * 16];

/* Interrupt Descriptor Table */
uint8_t idt[IDT_NUM_ENTRIES * 16];

/* Boot CPU pcpu structure */
static struct PCPU bsp_pcpu;

/* TSS used by the kernel */
struct TSS kernel_tss;

/* Bootinfo as supplied by the loader, or NULL */
struct BOOTINFO* bootinfo = NULL;
static struct BOOTINFO _bootinfo;

/* CPU clock speed, in MHz */
int md_cpu_clock_mhz = 0;

uint64_t
rdmsr(uint32_t msr)
{
	uint32_t hi, lo;

	__asm(
		"rdmsr\n"
	: "=a" (lo), "=d" (hi) : "c" (msr));
	return ((uint64_t)hi << 32) | (uint64_t)lo;
}

void
wrmsr(uint32_t msr, uint64_t val)
{
	__asm(
		"wrmsr\n"
	: : "a" (val & 0xffffffff),
	    "d" (val >> 32),
      "c" (msr));
}

void*
bootstrap_get_page()
{
	void* ptr = avail;
	memset(ptr, 0, PAGE_SIZE);
	avail += PAGE_SIZE;
	return ptr;
}

void
md_startup(struct BOOTINFO* bootinfo_ptr)
{
	/*
	 * This function will be called by the loader, which hands us a bootinfo
	 * structure containing a lot of useful information. We need it as soon
	 * as possible to bootstrap the VM and initialize the memory manager.
	 */
	if (bootinfo_ptr != NULL) {
		/*
		 * Copy enough bytes to cover our bootinfo - but only activate the bootinfo
	   * if the size is correct.
		 */
		memcpy(&_bootinfo, bootinfo_ptr, sizeof(struct BOOTINFO));
		if (bootinfo_ptr->bi_size >= sizeof(struct BOOTINFO))
			bootinfo = (struct BOOTINFO*)&_bootinfo;
	}

	if (bootinfo == NULL)
		panic("going nowhere without my bootinfo");

	/*
	 * First of all, initialize a new Global Descriptor Table; the trampoline
	 * adds a lot of now-useless fields which we can do without, and we'll need
	 * things like a TSS.
	 */
	memset(gdt, 0, sizeof(gdt));
	GDT_SET_CODE64(gdt, GDT_SEL_KERNEL_CODE, SEG_DPL_SUPERVISOR);
	GDT_SET_DATA64(gdt, GDT_SEL_KERNEL_DATA, SEG_DPL_SUPERVISOR);
	GDT_SET_CODE64(gdt, GDT_SEL_USER_CODE, SEG_DPL_USER);
	GDT_SET_DATA64(gdt, GDT_SEL_USER_DATA, SEG_DPL_USER);
	GDT_SET_TSS64 (gdt, GDT_SEL_TASK, 0, (addr_t)&kernel_tss, sizeof(struct TSS));
	MAKE_RREGISTER(gdtr, gdt, GDT_LENGTH - 1);

	/* Load the GDT, and reload our registers */
	__asm(
		"lgdt (%%rax)\n"
		"mov %%bx, %%ds\n"
		"mov %%bx, %%es\n"
		"mov %%bx, %%fs\n"
		"mov %%bx, %%gs\n"
		"mov %%bx, %%ss\n"
		/*
		 * All we need is 'mov %%cx, %%cs'; For the first time, directly editing
		 * CS is useful, so it can't be used :-)
		 */
		"pushq	%%rcx\n"
		"pushq	$l1\n"
		"lretq\n"
"l1:\n"
	: : "a" (&gdtr),
 	    "b" (GDT_SEL_KERNEL_DATA),
	    "c" (GDT_SEL_KERNEL_CODE));

	/*
	 * Construct the IDT; this ensures we can handle exceptions, which is useful
	 * and we'll want to have as soon as possible.
	 */
	memset(idt, 0, sizeof(idt));
	IDT_SET_ENTRY(idt,  0, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception0);
	IDT_SET_ENTRY(idt,  1, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception1);
	IDT_SET_ENTRY(idt,  2, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception2);
	IDT_SET_ENTRY(idt,  3, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception3);
	IDT_SET_ENTRY(idt,  4, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception4);
	IDT_SET_ENTRY(idt,  5, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception5);
	IDT_SET_ENTRY(idt,  6, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception6);
	IDT_SET_ENTRY(idt,  7, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception7);
	IDT_SET_ENTRY(idt,  8, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception8);
	IDT_SET_ENTRY(idt,  9, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception9);
	IDT_SET_ENTRY(idt, 10, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception10);
	IDT_SET_ENTRY(idt, 11, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception11);
	IDT_SET_ENTRY(idt, 12, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception12);
	IDT_SET_ENTRY(idt, 13, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception13);
	IDT_SET_ENTRY(idt, 14, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception14);
	IDT_SET_ENTRY(idt, 16, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception16);
	IDT_SET_ENTRY(idt, 17, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception17);
	IDT_SET_ENTRY(idt, 18, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception18);
	IDT_SET_ENTRY(idt, 19, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception19);
	IDT_SET_ENTRY(idt, 32, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, scheduler_irq);
	IDT_SET_ENTRY(idt, 33, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq1);
	IDT_SET_ENTRY(idt, 34, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq2);
	IDT_SET_ENTRY(idt, 35, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq3);
	IDT_SET_ENTRY(idt, 36, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq4);
	IDT_SET_ENTRY(idt, 37, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq5);
	IDT_SET_ENTRY(idt, 38, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq6);
	IDT_SET_ENTRY(idt, 39, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq7);
	IDT_SET_ENTRY(idt, 40, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq8);
	IDT_SET_ENTRY(idt, 41, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq9);
	IDT_SET_ENTRY(idt, 42, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq10);
	IDT_SET_ENTRY(idt, 43, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq11);
	IDT_SET_ENTRY(idt, 44, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq12);
	IDT_SET_ENTRY(idt, 45, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq13);
	IDT_SET_ENTRY(idt, 46, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq14);
	IDT_SET_ENTRY(idt, 47, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, irq15);

	/* Load the IDT */
	MAKE_RREGISTER(idtr, idt, (IDT_NUM_ENTRIES * 16) - 1);
	__asm(
		"lidt (%%rax)\n"
	: : "a" (&idtr));

	/*
	 * Remap the interrupts; by default, IRQ 0-7 are mapped to interrupts 0x08 -
	 * 0x0f and IRQ 8-15 to 0x70 - 0x77. We remap IRQ 0-15 to 0x20-0x2f (since
	 * 0..0x1f is reserved by Intel).
	 */
	x86_pic_remap();

	/* Set up the %fs base adress to zero (it should be, but don't take chances) */
	__asm("wrmsr\n" : : "a" (0), "d" (0), "c" (MSR_FS_BASE));

	/*
	 * Set up the kernel / current %gs base; this points to our per-cpu data; the
	 * current %gs base must be set because the interrupt code will not swap it
	 * if we came from kernel context.
	 */
	__asm(
		"wrmsr\n"
		"movl	%%ebx, %%ecx\n"
		"wrmsr\n"
	: : "a" (((addr_t)&bsp_pcpu) & 0xffffffff),
	    "d" (((addr_t)&bsp_pcpu) >> 32),
      "c" (MSR_KERNEL_GS_BASE),
      "b" (MSR_GS_BASE));
	
	/*
	 * Load the kernel TSS; this is still needed to switch stacks between
	 * ring 0 and 3 code.
	 */
	memset(&kernel_tss, 0, sizeof(struct TSS));
	__asm("ltr %%ax\n" : : "a" (GDT_SEL_TASK));

	/*
	 * Set up the fast system call (SYSCALL/SYSEXIT) mechanism.
	 */
	wrmsr(MSR_EFER, rdmsr(MSR_EFER) | MSR_EFER_SCE);
	wrmsr(MSR_STAR, ((uint64_t)(GDT_SEL_USER_CODE - 0x10) | SEG_DPL_USER) << 48L |
                  ((uint64_t)GDT_SEL_KERNEL_CODE << 32L));
extern void* syscall_handler;
	wrmsr(MSR_LSTAR, (addr_t)&syscall_handler);
	wrmsr(MSR_SFMASK, 0x200 /* IF */);

	/*
	 * The loader tells us how large the kernel is; we use pages directly after
	 * the kernel.
	 */
	extern void *__entry, *__end;
	avail = (void*)PTOKV(((struct LOADER_MODULE*)(addr_t)bootinfo_ptr->bi_modules)->mod_phys_end_addr);
	if ((addr_t)avail % PAGE_SIZE > 0)
		avail += (PAGE_SIZE - ((addr_t)avail % PAGE_SIZE));
	pml4 = (uint64_t*)bootstrap_get_page();

	/* First of all, map the kernel; we won't get far without it */
	addr_t from = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t   to = ((addr_t)&__end | (PAGE_SIZE - 1)) + 1;
	to += 1048576; /* XXX */
	vm_mapto(from, from & 0x0fffffff /* HACK */, (to - from) / PAGE_SIZE);

	__asm(
		/* Set CR3 to the physical address of the page directory */
		"movq %%rax, %%cr3\n"
	: : "a" ((addr_t)pml4 & 0x0fffffff /* HACK */));

	/* Walk the memory map, and add each item one by one */
	kprintf("bootinfo=%p; bi_map=%x, bi_map_size=%x\n",
	 bootinfo, bootinfo->bi_memory_map_addr, bootinfo->bi_memory_map_size);
	if (bootinfo != NULL && bootinfo->bi_memory_map_addr != (addr_t)NULL &&
	    bootinfo->bi_memory_map_size > 0 &&
	    (bootinfo->bi_memory_map_size % sizeof(struct SMAP_ENTRY)) == 0) {
		int mem_map_pages = (bootinfo->bi_memory_map_size + PAGE_SIZE - 1) / PAGE_SIZE;
		void* smap = vm_map_kernel((addr_t)bootinfo->bi_memory_map_addr, mem_map_pages, VM_FLAG_READ);
		struct SMAP_ENTRY* smap_entry = smap;
		for (int i = 0; i < bootinfo->bi_memory_map_size / sizeof(struct SMAP_ENTRY); i++, smap_entry++) {
			if (smap_entry->type != SMAP_TYPE_MEMORY)
				continue;

			/* This piece of memory is available; add it */
			addr_t base = (addr_t)smap_entry->base_hi << 32 | smap_entry->base_lo;
			size_t len = (size_t)smap_entry->len_hi << 32 | smap_entry->len_lo;
			base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			len  &= ~(PAGE_SIZE - 1);
			mm_zone_add(base, len);
		}
		vm_unmap_kernel((addr_t)smap, mem_map_pages);
	} else {
		/* Loader did not provide a sensible memory map - now what? */
		panic("No memory map!");
	}

	/* We have memory - initialize our VM */
	vm_init();

	/*
	 * Initialize the FPU. We assume that any CPU that can do Long Mode will also
	 * support SSE/SSE2 instructions.
	 *
	 * XXX should we check whether this stuff is supported ?
	 */
	__asm(
		"movq	%cr4, %rax\n"
		"orq	$0x600, %rax\n"		/* OSFXSR | OSXMMEXCPT */
		"movq	%rax, %cr4\n"
	);

	/*
	 * Mark the pages occupied by the kernel as used; this prevents
	 * the memory allocator from handing out memory where the kernel
	 * lives.
	 */
	kmem_mark_used((void*)((addr_t)from & 0x0fffffff) /* HACK */, (to - from) / PAGE_SIZE);

	/* Initialize the handles; this is needed by the per-CPU code as it initialize threads */
	handle_init();

	/*
	 * Initialize the per-CPU thread; this needs a working memory allocator, so that is why
	 * we delay it.
	 */
	memset(&bsp_pcpu, 0, sizeof(bsp_pcpu));
	pcpu_init(&bsp_pcpu);

	/*
	 * Enable interrupts. We do this right before the machine-independant code
	 * because it will allow us to rely on interrupts when probing devices etc.
	 *
	 * Note that we explicitely block the scheduler, as this only should be
	 * enabled once we are ready to run userland threads.
	 */
	__asm("sti");

	/* Find out how quick the CPU is; this requires interrupts and will be needed for delay() */
	md_cpu_clock_mhz = x86_pit_calc_cpuspeed_mhz();

	/* All done - it's up to the machine-independant code now */
	mi_startup();
}

void
vm_map_kernel_addr(void* pml4)
{
	extern void *__entry, *__end;
	addr_t from = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t   to = (addr_t)&__end;
	to += PAGE_SIZE - to % PAGE_SIZE;
	
	vm_mapto_pagedir(pml4, from, from & 0x0fffffff /* HACK */, (to - from) / PAGE_SIZE, 1);
}

/* vim:set ts=2 sw=2: */
