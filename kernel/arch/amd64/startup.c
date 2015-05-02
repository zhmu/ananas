#include <ananas/types.h>
#include <machine/param.h>
#include <machine/macro.h>
#include <machine/interrupts.h>
#include <machine/vm.h>
#include <machine/pcpu.h>
#include <machine/thread.h>
#include <ananas/x86/pic.h>
#include <ananas/x86/pit.h>
#include <ananas/x86/smap.h>
#include <ananas/handle.h>
#include <ananas/bootinfo.h>
#include <ananas/kmem.h>
#include <ananas/init.h>
#include <ananas/vm.h>
#include <ananas/pcpu.h>
#include <ananas/mm.h>
#include <ananas/lib.h>
#include <loader/module.h>

/* Pointer to the page directory level 4 */
uint64_t* kernel_pagedir;

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

#define PHYSMEM_NUM_CHUNKS 32
struct PHYSMEM_CHUNK {
	addr_t addr, len;
};
static struct PHYSMEM_CHUNK phys[PHYSMEM_NUM_CHUNKS];

/* CPU clock speed, in MHz */
int md_cpu_clock_mhz = 0;

static inline uint64_t
rdmsr(uint32_t msr)
{
	uint32_t hi, lo;

	__asm(
		"rdmsr\n"
	: "=a" (lo), "=d" (hi) : "c" (msr));
	return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static inline void
wrmsr(uint32_t msr, uint64_t val)
{
	__asm(
		"wrmsr\n"
	: : "a" (val & 0xffffffff),
	    "d" (val >> 32),
      "c" (msr));
}

static void*
bootstrap_get_pages(addr_t* avail, size_t num)
{
	void* ptr = (void*)*avail;
	memset(ptr, 0, num * PAGE_SIZE);
	*avail += num * PAGE_SIZE;
	return ptr;
}

static void
setup_paging(addr_t* avail, size_t mem_size, size_t kernel_size)
{
#define KMAP_KVA_START KMEM_DIRECT_VA_START
#define KMAP_KVA_END KMEM_DYNAMIC_VA_END
	/*
	 * Taking the overview in machine/vm.h into account, we want to map the
	 * following regions:
	 *
	 * - KMAP_KVA_START .. KMAP_KVA_END: the kernel's KVA
	 *   This may be mapped as 4KB pages; however, we can lower the estimate if
	 *   there is less memory available than the total size of this region.
	 * - KERNBASE ... KERNEND: the kernel code/data
	 *   We always map this as 4KB pages to ensure we can benefit most optimally
	 *   from NX.
	 */
	uint64_t kmap_kva_end = KMAP_KVA_END;
	if (kmap_kva_end - KMAP_KVA_START >= mem_size) {
		/* Tone down KVA as we have less memory */
		kmap_kva_end = KMAP_KVA_START + mem_size;
	}

	// XXX Ensure we'll have at least 4GB KVA; this is needed in order to map PCI
	// devices (we should actually just add specific mappings for this place instead)
	if (mem_size < 4UL * 1024 * 1024 * 1024)
		kmap_kva_end = KMAP_KVA_START + 4UL * 1024 * 1024 * 1024;

	uint64_t kva_size = kmap_kva_end - KMAP_KVA_START;
	/* Number of level-4 page-table entries required for the KVA */
	unsigned int kva_num_pml4e = (kva_size + (1ULL << 39) - 1) >> 39;
	/* Number of level-3 page-table entries required for the KVA */
	unsigned int kva_num_pdpe = (kva_size + (1ULL << 30) - 1) >> 30;
	/* Number of level-2 page-table entries required for the KVA */
	unsigned int kva_num_pde = (kva_size + (1ULL << 21) - 1) >> 21;
	/* Number of level-1 page-table entries required for the KVA */
	unsigned int kva_num_pte = (kva_size + (1ULL << 12) - 1) >> 12;
	kprintf("kva_size=%p => kva_num_pml4e=%d kva_num_pdpe=%d kva_num_pde=%d kva_num_pte=%d\n", kva_size, kva_num_pml4e, kva_num_pdpe, kva_num_pde, kva_num_pte);

	/* Grab memory for all kernel KVA */
	void* l4p = bootstrap_get_pages(avail, kva_num_pml4e);
	void* l3p = bootstrap_get_pages(avail, kva_num_pdpe);
	void* l2p = bootstrap_get_pages(avail, kva_num_pde);

	/* Finally, allocate the kernel pagedir itself */
	kernel_pagedir = (uint64_t*)bootstrap_get_pages(avail, 1);
	kprintf(">>> kernel_pagedir = %p\n", kernel_pagedir);

	addr_t avail_start = (((struct LOADER_MODULE*)(addr_t)bootinfo->bi_modules)->mod_phys_end_addr);
	avail_start = (avail_start | (PAGE_SIZE - 1)) + 1;

	/*
	 * Calculate the number of entries we need for the kernel itself; we just
	 * need to grab the bare necessities as we'll map all other memory using the
	 * KVA instead. We do this here to update the 'avail' pointer to ensure
	 * we'll map these tables in KVA as well, should we ever need to change them.
	 *
	 * XXX We could consider mapping the kernel using 2MB pages if the NX bits align...
 	 */
	unsigned int kernel_num_pml4e = (kernel_size + (1ULL << 39) - 1) >> 39;
	unsigned int kernel_num_pdpe = (kernel_size + (1ULL << 30) - 1) >> 30;
	unsigned int kernel_num_pde = (kernel_size + (1ULL << 21) - 1) >> 21;
	unsigned int kernel_num_pte = (kernel_size + (1ULL << 12) - 1) >> 12;

	uint64_t* kernel_pml4e = (uint64_t*)bootstrap_get_pages(avail, kernel_num_pml4e);
	uint64_t* kernel_pdpe = (uint64_t*)bootstrap_get_pages(avail, kernel_num_pdpe);
	uint64_t* kernel_pde = (uint64_t*)bootstrap_get_pages(avail, kernel_num_pde);

	/*
	 * Map the KVA - we will only map what we have used for our page tables, to
	 * ensure we can change them later as necessary. We explicitly won't map the
	 * kernel page tables here because we never need to change them.
	 */
	addr_t kva_addr = KMAP_KVA_START;
	addr_t phys = 0;
	for (unsigned int n = 0; n < kva_num_pte; n++) {
		uint64_t* pml4e = &kernel_pagedir[(kva_addr >> 39) & 0x1ff];
		uint64_t* p = l4p + (n >> 27) * PAGE_SIZE;
		if (*pml4e == 0)
			*pml4e = (addr_t)p | PE_RW | PE_P | PE_C_G;
		uint64_t* pdpe = &p[(kva_addr >> 30) & 0x1ff];
		uint64_t* q = l3p + (n >> 18) * PAGE_SIZE;
		if (*pdpe == 0)
			*pdpe = (addr_t)q | PE_RW | PE_P | PE_C_G;
		uint64_t* pde = &q[(kva_addr >> 21) & 0x1ff];
		uint64_t* r = l2p + (n >> 9) * PAGE_SIZE;
		if (*pde == 0)
			*pde = (addr_t)r | PE_RW | PE_P | PE_C_G;

		if (phys >= avail_start && phys <= (addr_t)*avail)
			r[n & 0x1ff] = phys | PE_G | PE_RW | PE_P;
		kva_addr += PAGE_SIZE;
		phys += PAGE_SIZE;
	}

	/* Now map the kernel itself */
	kprintf("kernel_num_pml4e = %d, kernel_num_pdpe = %d, kernel_num_pde = %d, kernel_num_pte = %d\n",
	 kernel_num_pml4e, kernel_num_pdpe, kernel_num_pde, kernel_num_pte);

	extern void *__entry, *__rodata_end;
	addr_t kernel_addr = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t kernel_text_end  = (addr_t)&__rodata_end & ~(PAGE_SIZE - 1);

	addr_t phys_addr = kernel_addr & 0x00ffffff;
	for (unsigned int n = 0; n < kernel_num_pte; n++) {
		uint64_t* pml4e = &kernel_pagedir[(kernel_addr >> 39) & 0x1ff];
		uint64_t* p = &kernel_pml4e[n >> 27];
		if (*pml4e == 0)
			*pml4e = (addr_t)p | PE_C_G | PE_RW | PE_P;
		uint64_t* pdpe = &p[(kernel_addr >> 30) & 0x1ff];
		uint64_t* q = &kernel_pdpe[(n >> 18) & 0x1ff];
		if (*pdpe == 0)
			*pdpe = (addr_t)q | PE_C_G | PE_RW | PE_P;
		uint64_t* pde = &q[(kernel_addr >> 21) & 0x1ff];
		uint64_t* r = &kernel_pde[(n >> 9) & 0x1ff];
		if (*pde == 0)
			*pde = (addr_t)r | PE_C_G | PE_RW | PE_P;

		/* Map kernel page; but code is always RO */
		unsigned int flags = PE_P | PE_G;
		if (kernel_addr > kernel_text_end)
			flags |= PE_RW;
		r[(kernel_addr >> 12) & 0x1ff] = phys_addr | flags;

		kernel_addr += PAGE_SIZE;
		phys_addr += PAGE_SIZE;
	}

	/* Enable global pages */
	__asm(
		"movq	%cr4, %rax\n"
		"orq	$0x80, %rax\n"		/* PGE */
		"movq	%rax, %cr4\n"
	);

	/* Activate our new page tables */
	kprintf(">>> activating kernel_pagedir = %p\n", kernel_pagedir);
	__asm(
		"movq %0, %%cr3\n"
	: : "r" (kernel_pagedir));

	/*
	 * Now we must update the kernel pagedir pointer to the new location, as that
	 * we have paging in place.
	 */
	kernel_pagedir = (uint64_t*)PTOKV((addr_t)kernel_pagedir);
	kprintf(">>> kernel_pagedir = %p\n", kernel_pagedir);
	kprintf(">>> *kernel_pagedir = %p\n", *kernel_pagedir);
}

void
md_startup(struct BOOTINFO* bootinfo_ptr)
{
	/*
	 * This function will be called by the loader, which hands us a bootinfo
	 * structure containing a lot of useful information. We need it as soon
	 * as possible to bootstrap the VM and initialize the memory manager.
	 *
	 * Note that this implicitely assumes that the bootinfo memory is mapped -
	 * indeed, we'll assume that the entire 1GB memory space is mapped. This
	 * makes setting up our page tables much easier as we'll always have enough
	 * space to store them.
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
	 * Initialize a new Global Descriptor Table; the trampoline adds a lot of
	 * now-useless fields which we can do without, and we'll need things like a
	 * TSS.
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
		/* Jump to our new %cs */
		"pushq	%%rcx\n"
		"pushq	$1f\n"
		"lretq\n"
"1:\n"
	: : "a" (&gdtr),
 	    "b" (GDT_SEL_KERNEL_DATA),
	    "c" (GDT_SEL_KERNEL_CODE));

	/* Ask the PIC to mask everything; we'll initialize when we are ready */
	x86_pic_mask_all();

	/*
	 * Construct the IDT; this ensures we can handle exceptions, which is useful
	 * and we'll want to have as soon as possible.
	 */
	memset(idt, 0, sizeof(idt));
	IDT_SET_ENTRY( 0, SEG_TGATE_TYPE, 0, exception0);
	IDT_SET_ENTRY( 1, SEG_TGATE_TYPE, 0, exception1);
	IDT_SET_ENTRY( 2, SEG_TGATE_TYPE, 0, exception2);
	IDT_SET_ENTRY( 3, SEG_TGATE_TYPE, 0, exception3);
	IDT_SET_ENTRY( 4, SEG_TGATE_TYPE, 0, exception4);
	IDT_SET_ENTRY( 5, SEG_TGATE_TYPE, 0, exception5);
	IDT_SET_ENTRY( 6, SEG_TGATE_TYPE, 0, exception6);
	IDT_SET_ENTRY( 7, SEG_TGATE_TYPE, 0, exception7);
	IDT_SET_ENTRY( 8, SEG_TGATE_TYPE, 0 /* 1 */, exception8); /* use IST1 for double fault */
	IDT_SET_ENTRY( 9, SEG_TGATE_TYPE, 0, exception9);
	IDT_SET_ENTRY(10, SEG_TGATE_TYPE, 0, exception10);
	IDT_SET_ENTRY(11, SEG_TGATE_TYPE, 0, exception11);
	IDT_SET_ENTRY(12, SEG_TGATE_TYPE, 0, exception12);
	IDT_SET_ENTRY(13, SEG_TGATE_TYPE, 0, exception13);
	/*
	 * Page fault must disable interrupts to ensure %cr2 (fault address) will not
	 * be overwritten; it will re-enable the interrupt flag when it's safe to do
	 * so.
	 */
	IDT_SET_ENTRY(14, SEG_IGATE_TYPE, 0, exception14);
	IDT_SET_ENTRY(16, SEG_TGATE_TYPE, 0, exception16);
	IDT_SET_ENTRY(17, SEG_TGATE_TYPE, 0, exception17);
	IDT_SET_ENTRY(18, SEG_TGATE_TYPE, 0, exception18);
	IDT_SET_ENTRY(19, SEG_TGATE_TYPE, 0, exception19);
	/* Use interrupt gates for IRQ's so that we can keep track of the nesting */
	IDT_SET_ENTRY(32, SEG_IGATE_TYPE, 0, irq0);
	IDT_SET_ENTRY(33, SEG_IGATE_TYPE, 0, irq1);
	IDT_SET_ENTRY(34, SEG_IGATE_TYPE, 0, irq2);
	IDT_SET_ENTRY(35, SEG_IGATE_TYPE, 0, irq3);
	IDT_SET_ENTRY(36, SEG_IGATE_TYPE, 0, irq4);
	IDT_SET_ENTRY(37, SEG_IGATE_TYPE, 0, irq5);
	IDT_SET_ENTRY(38, SEG_IGATE_TYPE, 0, irq6);
	IDT_SET_ENTRY(39, SEG_IGATE_TYPE, 0, irq7);
	IDT_SET_ENTRY(40, SEG_IGATE_TYPE, 0, irq8);
	IDT_SET_ENTRY(41, SEG_IGATE_TYPE, 0, irq9);
	IDT_SET_ENTRY(42, SEG_IGATE_TYPE, 0, irq10);
	IDT_SET_ENTRY(43, SEG_IGATE_TYPE, 0, irq11);
	IDT_SET_ENTRY(44, SEG_IGATE_TYPE, 0, irq12);
	IDT_SET_ENTRY(45, SEG_IGATE_TYPE, 0, irq13);
	IDT_SET_ENTRY(46, SEG_IGATE_TYPE, 0, irq14);
	IDT_SET_ENTRY(47, SEG_IGATE_TYPE, 0, irq15);

	/* Load the IDT */
	MAKE_RREGISTER(idtr, idt, (IDT_NUM_ENTRIES * 16) - 1);
	__asm(
		"lidt (%%rax)\n"
	: : "a" (&idtr));

	/* Set up the userland %fs/%gs base adress to zero (it should be, but don't take chances) */
	wrmsr(MSR_FS_BASE, 0);
	wrmsr(MSR_GS_BASE, 0);

	/*
	 * Set up the kernel / current %gs base; this points to our per-cpu data; the
	 * current %gs base must be set because the interrupt code will not swap it
	 * if we came from kernel context.
	 */
	wrmsr(MSR_GS_BASE, (addr_t)&bsp_pcpu); // XXX Fixen crasht zonder dit
	wrmsr(MSR_KERNEL_GS_BASE, (addr_t)&bsp_pcpu);
	
	/*
	 * Load the kernel TSS; this is still needed to switch stacks between
	 * ring 0 and 3 code.
	 */
	memset(&kernel_tss, 0, sizeof(struct TSS));
extern void* bootstrap_stack;
	kernel_tss.ist1 = (addr_t)&bootstrap_stack;
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
	 * Determine how much memory we have; we need this in order to pre-allocate
	 * our page tables.
	 */
	if (bootinfo->bi_memory_map_addr == (addr_t)NULL ||
	    bootinfo->bi_memory_map_size == 0 ||
	    (bootinfo->bi_memory_map_size % sizeof(struct SMAP_ENTRY)) != 0)
		panic("going nowhere without a memory map");

	/*
	 * The loader tells us how large the kernel is; we use pages directly after
	 * the kernel.
	 */
	extern void *__entry, *__end;
	addr_t avail = (addr_t)(((struct LOADER_MODULE*)(addr_t)bootinfo_ptr->bi_modules)->mod_phys_end_addr);
	avail = (addr_t)((addr_t)avail | (PAGE_SIZE - 1)) + 1;
	kprintf("avail = %p\n", avail);

	/*
	 * Figure out where the kernel is in physical memory; we must exclude it from
	 * the memory maps.
	 */
	addr_t kernel_from = ((addr_t)&__entry - KERNBASE) & ~(PAGE_SIZE - 1);
	addr_t kernel_to = (((addr_t)&__end - KERNBASE) | (PAGE_SIZE - 1)) + 1;

	/* Now build the memory chunk list */
	int phys_idx = 0;

	size_t mem_size = 0;
	struct SMAP_ENTRY* smap_entry = (void*)(uintptr_t)bootinfo->bi_memory_map_addr;
	for (int i = 0; i < bootinfo->bi_memory_map_size / sizeof(struct SMAP_ENTRY); i++, smap_entry++) {
		if (smap_entry->type != SMAP_TYPE_MEMORY)
			continue;

		/* This piece of memory is available; add it */
		addr_t base = (addr_t)smap_entry->base_hi << 32 | smap_entry->base_lo;
		size_t len = (size_t)smap_entry->len_hi << 32 | smap_entry->len_lo;
		base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		len  &= ~(PAGE_SIZE - 1);

		/* See if this chunk collides with our kernel; if it does, add it in 0 .. 2 slices */
#define MAX_SLICES 2
		addr_t start[MAX_SLICES], end[MAX_SLICES];
		kmem_chunk_reserve(base, base + len, kernel_from, kernel_to, &start[0], &end[0]);
		for (unsigned int n = 0; n < MAX_SLICES; n++) {
			KASSERT(start[n] <= end[n] && end[n] >= start[n], "invalid start/end pair %p/%p", start[n], end[n]);
			if (start[n] == end[n])
				continue;
			// XXX We should check to make sure we don't go beyond phys_... here
			phys[phys_idx].addr = start[n];
			phys[phys_idx].len = end[n] - start[n];
			mem_size += phys[phys_idx].len;
			phys_idx++;
		}
#undef MAX_SLICES
	}
	KASSERT(phys_idx > 0, "no physical memory chunks");

	/* Dump the physical memory layout as far as we know it */
	kprintf("physical memory dump: %d chunk(s), %d MB\n", phys_idx, mem_size / (1024 * 1024));
	for (int n = 0; n < phys_idx; n++) {
		struct PHYSMEM_CHUNK* p = &phys[n];
		kprintf("  chunk %d: %016p-%016p (%u KB)\n",
		 n, p->addr, p->addr + p->len - 1,
		 p->len / 1024);
	}
	uint64_t prev_avail = avail;
	setup_paging(&avail, mem_size, kernel_to - kernel_from);

	/*
	 * Now add the physical chunks of memory. Note that phys[] isn't up-to-date
	 * anymore as it will contain things we have used for the page tables - this
	 * is why we store prev_avail: it's always at the start of a chunk, and we can
	 * just update the chunk as needed.
	 */
	for (int n = 0; n < phys_idx; n++) {
		struct PHYSMEM_CHUNK* p = &phys[n];
		if (p->addr == prev_avail) {
			kprintf("altering prev chunk\n");
			p->addr = avail;
			p->len -= avail - prev_avail;
		}
		kprintf("+adding memory chunk %p (%p)\n", p->addr, p->len);
		page_zone_add(p->addr, p->len);
		kprintf("-adding memory chunk %p (%p)\n", p->addr, p->len);
	}

	/* We have memory - initialize our VM */
	vm_init();

	/* Enable fast system calls */
	__asm(
		"movq	%cr4, %rax\n"
		"orq	$0x600, %rax\n"		/* OSFXSR | OSXMMEXCPT */
		"movq	%rax, %cr4\n"
	);

	/* Enable the memory code - handle_init() calls kmalloc() */
	mm_init();

	/* Initialize the handles; this is needed by the per-CPU code as it initialize threads */
	handle_init();

	/*
	 * Initialize the per-CPU thread; this needs a working memory allocator, so that is why
	 * we delay it.
	 */
	memset(&bsp_pcpu, 0, sizeof(bsp_pcpu));
	pcpu_init(&bsp_pcpu);
	bsp_pcpu.tss = (addr_t)&kernel_tss;

	/*
	 * Switch to the idle thread - the reason we do it here is because it removes the
	 * curthread==NULL case and it will has a larger stack than our bootstrap stack. The
	 * bootstrap stack is re-used as double fault stack.
	 */
	__asm __volatile(
		"movq %0, %%rsp\n"
	: : "r" (bsp_pcpu.idlethread->md_rsp));
	PCPU_SET(curthread, bsp_pcpu.idlethread);
	scheduler_add_thread(bsp_pcpu.idlethread);

	/*
	 * XXX No SMP here yet, so use the PIC for now.
	 */
	x86_pic_init();

	/* Initialize the PIT */
	x86_pit_init();

	/*
	 * Enable interrupts. We do this right before the machine-independant code
	 * because it will allow us to rely on interrupts when probing devices etc.
	 *
	 * Note that we explicitely block the scheduler, as this only should be
	 * enabled once we are ready to run userland threads.
	 */
	md_interrupts_enable();

	/* Find out how quick the CPU is; this requires interrupts and will be needed for delay() */
	md_cpu_clock_mhz = x86_pit_calc_cpuspeed_mhz();

	/* All done - it's up to the machine-independant code now */
	mi_startup();
}

/* vim:set ts=2 sw=2: */
