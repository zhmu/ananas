#include "i386/types.h"
#include "i386/vm.h"
#include "i386/io.h"
#include "i386/interrupts.h"
#include "i386/macro.h"
#include "i386/thread.h"
#include "i386/pcpu.h"
#include "i386/realmode.h"
#include "x86/pic.h"
#include "init.h"
#include "lib.h"
#include "options.h"
#include "pcpu.h"
#include "mm.h"
#include "param.h"
#include "vm.h"

/* __end is defined by the linker script and indicates the end of the kernel */
extern void* __end;

/* __entry is the entry position and the first symbol in the kernel */
extern void* __entry;

/* Pointer to our page directory */
uint32_t* pagedir;

/* Global/Interrupt descriptor tables, as preallocated by stub.s */
extern void *idt, *gdt;

/* Used for temporary mappings during VM bootstrap */
extern void* temp_pt_entry;

/* Initial TSS used by the kernel */
struct TSS kernel_tss;

/* Boot CPU pcpu structure */
static struct PCPU bsp_pcpu;

/*
 * i386-dependant startup code, called by stub.s.
 */
void
md_startup()
{
	uint32_t i;

	/*
	 * Once this is called, we don't have any paging set up yet. This
	 * means that we can't access any variables and/or data without
	 * substracting KERNBASE.
	 *
	 * First of all, we figure out where the kernel ends and thus space is
	 * available; this will already be rounded to a page.
	 */
	addr_t avail = (addr_t)&__end - KERNBASE;

	/*
	 * Paging on i386 works by having a 'page directory' (PD), which consists of
	 * 1024 pointers to a 'page table' (PT). While resolving any 32-bit address
	 * (with bits ordered from highest 31 to 0), the top 10 bits (22-31) refer to
	 * which page directory to use, while the next 10 bits (12-21) refer to the
	 * page table to use and finally, the lower 12 bits (11-0) are the address
	 * itself.
	 *
	 * We place the PD directly after the kernel itself, and follow this by any
	 * PT's we need to map the virtual kernel addresses.
	 */
	uint32_t* pd = (uint32_t*)avail;
	avail += PAGE_SIZE;
	memset((void*)pd, 0, PAGE_SIZE);

	/*
	 * Now, walk through the memory occupied by the kernel and map the
	 * pages. Note that the new page tables are placed after the kernel,
	 * so we need to map these too (otherwise, we cannot update our
	 * pagetables anymore once they are in place)
	 */
	addr_t last_addr = (addr_t)&__end;
	for (i = (addr_t)&__entry; i < last_addr; i += PAGE_SIZE) {
		uint32_t pd_entrynum = i >> 22;
		if (pd[pd_entrynum] == 0) {
			/* There's no directory entry yet for this item, so create one */
			pd[pd_entrynum] = avail | PDE_P | PTE_RW;
			memset((void*)avail, 0, PAGE_SIZE);
			avail += PAGE_SIZE;
			last_addr += PAGE_SIZE;
		}

		/*
		 * Determine the index within the page table; this is simply bits
	 	 * 12-21, so we need to shift the lower 12 bits away and throw every
	 	 * above bit 10 away.
		 */
		uint32_t* pt = (uint32_t*)(pd[pd_entrynum] & ~(PAGE_SIZE - 1));
		pt[(((i >> 12) & ((1 << 10) - 1)))] = (i - KERNBASE) | PTE_P | PTE_RW;
	}

	/*
	 * OK, the page table we have constructed should be valid for the kernel's
	 * virtual addresses. However, if we would activate this table, the
	 * system would immediately crash because the address we are executing is
	 * no longer mapped!
	 *
	 * Resolve this by duplicating the PD mappings to KERNBASE - __end to the
	 * mappings minus KERNBASE.
	 */
	vm_map_kernel_lowaddr(pd);

	/* It's time to... throw the switch! */
	__asm(
		/* Set CR3 to the physical address of the page directory */
		"movl	%%eax, %%cr3\n"
		"jmp	l1\n"
"l1:\n"
		/* Enable paging by flipping the PG bit in CR0 */
		"movl	%%cr0, %%eax\n"
		"orl	$0x80000000, %%eax\n"
		"movl	%%eax, %%cr0\n"
		"jmp	l2\n"
"l2:\n"
		/* Finally, jump to our virtual address */
		"movl	$l3, %%eax\n"
		"push	%%eax\n"
		"ret\n"
"l3:\n"
		/* Update ESP/EBP to use the new virtual addresses */
		"addl	%%ebx, %%esp\n"
		"addl	%%ebx, %%ebp\n"
	: : "a" (pd), "b" (KERNBASE));

	/*
	 * We can throw the duplicate mappings away now, since we are now
	 * using our virtual mappings.
	 */
	vm_unmap_kernel_lowaddr(pd);

	__asm(
		/* Reload the page directory */
		"movl	%%eax, %%cr3\n"
		"jmp	l4\n"
"l4:\n"
	: : "a" (pd));

	/*
	 * Paging has been setup; this means we can sensibly use kernel memory now.
	 */
	pagedir = (uint32_t*)((addr_t)pd | KERNBASE);

	/*
	 * The next step is to set up the Global Descriptor Table (GDT); this is
	 * used to distinguish between kernel and userland code, as well as being able
	 * to switch between tasks.
	 */

	/*
	 * Prepare the GDT entries; this is just kernel/user code/data - protection between
	 * tasks is handeled using paging.
	 */
	memset(&gdt, 0, GDT_NUM_ENTRIES * 8);
	addr_t stub16 = (addr_t)&realmode16_stub - KERNBASE;
	addr_t bsp_addr = (addr_t)&bsp_pcpu;
	GDT_SET_ENTRY32(&gdt, GDT_IDX_KERNEL_CODE,   SEG_TYPE_CODE, SEG_DPL_SUPERVISOR, 0, 0xfffff);
	GDT_SET_ENTRY32(&gdt, GDT_IDX_KERNEL_DATA,   SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, 0, 0xfffff);
	GDT_SET_ENTRY16(&gdt, GDT_IDX_KERNEL_CODE16, SEG_TYPE_CODE, SEG_DPL_SUPERVISOR, stub16);
	GDT_SET_ENTRY16(&gdt, GDT_IDX_KERNEL_DATA16, SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, stub16);
	GDT_SET_ENTRY32(&gdt, GDT_IDX_KERNEL_PCPU,   SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, bsp_addr, sizeof(struct PCPU));
	GDT_SET_ENTRY32(&gdt, GDT_IDX_USER_CODE,     SEG_TYPE_CODE, SEG_DPL_USER,       0, 0xfffff);
	GDT_SET_ENTRY32(&gdt, GDT_IDX_USER_DATA,     SEG_TYPE_DATA, SEG_DPL_USER,       0, 0xfffff);
	GDT_SET_TSS(&gdt, GDT_IDX_KERNEL_TASK, 0, (addr_t)&kernel_tss, sizeof(struct TSS));

	MAKE_RREGISTER(gdtr, &gdt, GDT_NUM_ENTRIES);

	/* Load the GDT, and reload our registers */
	__asm(
		"lgdt (%%eax)\n"
		"mov %%bx, %%ds\n"
		"mov %%bx, %%es\n"
		"mov %%dx, %%fs\n"
		"mov %%bx, %%gs\n"
		"pushl %%ecx\n"
		"pushl $l5\n"
		"lret\n"		/* retf */
"l5:\n"
	: : "a" (&gdtr),
 	    "b" (GDT_SEL_KERNEL_DATA),
	    "c" (GDT_SEL_KERNEL_CODE),
	    "d" (GDT_SEL_KERNEL_PCPU));

	/*
	 * Remap the interrupts; by default, IRQ 0-7 are mapped to interrupts 0x08 -
	 * 0x0f and IRQ 8-15 to 0x70 - 0x77. We remap IRQ 0-15 to 0x20-0x2f (since
	 * 0..0x1f is reserved by Intel).
	 */
	x86_pic_remap();

	/*
	 * Prepare the IDT entries; this means mapping the exception interrupts to
	 * handlers.
	 */
	memset(&idt, 0, IDT_NUM_ENTRIES * 8);
	IDT_SET_ENTRY(0x00, SEG_DPL_SUPERVISOR, exception0);
	IDT_SET_ENTRY(0x01, SEG_DPL_SUPERVISOR, exception1);
	IDT_SET_ENTRY(0x02, SEG_DPL_SUPERVISOR, exception2);
	IDT_SET_ENTRY(0x03, SEG_DPL_SUPERVISOR, exception3);
	IDT_SET_ENTRY(0x04, SEG_DPL_SUPERVISOR, exception4);
	IDT_SET_ENTRY(0x05, SEG_DPL_SUPERVISOR, exception5);
	IDT_SET_ENTRY(0x06, SEG_DPL_SUPERVISOR, exception6);
	IDT_SET_ENTRY(0x07, SEG_DPL_SUPERVISOR, exception7);
	IDT_SET_ENTRY(0x08, SEG_DPL_SUPERVISOR, exception8);
	IDT_SET_ENTRY(0x09, SEG_DPL_SUPERVISOR, exception9);
	IDT_SET_ENTRY(0x0a, SEG_DPL_SUPERVISOR, exceptionA);
	IDT_SET_ENTRY(0x0b, SEG_DPL_SUPERVISOR, exceptionB);
	IDT_SET_ENTRY(0x0c, SEG_DPL_SUPERVISOR, exceptionC);
	IDT_SET_ENTRY(0x0d, SEG_DPL_SUPERVISOR, exceptionD);
	IDT_SET_ENTRY(0x0e, SEG_DPL_SUPERVISOR, exceptionE);
	IDT_SET_ENTRY(0x20, SEG_DPL_SUPERVISOR, scheduler_irq);
	IDT_SET_ENTRY(0x21, SEG_DPL_SUPERVISOR, irq1);
	IDT_SET_ENTRY(0x22, SEG_DPL_SUPERVISOR, irq2);
	IDT_SET_ENTRY(0x23, SEG_DPL_SUPERVISOR, irq3);
	IDT_SET_ENTRY(0x24, SEG_DPL_SUPERVISOR, irq4);
	IDT_SET_ENTRY(0x25, SEG_DPL_SUPERVISOR, irq5);
	IDT_SET_ENTRY(0x26, SEG_DPL_SUPERVISOR, irq6);
	IDT_SET_ENTRY(0x27, SEG_DPL_SUPERVISOR, irq7);
	IDT_SET_ENTRY(0x28, SEG_DPL_SUPERVISOR, irq8);
	IDT_SET_ENTRY(0x29, SEG_DPL_SUPERVISOR, irq9);
	IDT_SET_ENTRY(0x2a, SEG_DPL_SUPERVISOR, irqA);
	IDT_SET_ENTRY(0x2b, SEG_DPL_SUPERVISOR, irqB);
	IDT_SET_ENTRY(0x2c, SEG_DPL_SUPERVISOR, irqC);
	IDT_SET_ENTRY(0x2d, SEG_DPL_SUPERVISOR, irqD);
	IDT_SET_ENTRY(0x2e, SEG_DPL_SUPERVISOR, irqE);
	IDT_SET_ENTRY(0x2f, SEG_DPL_SUPERVISOR, irqF);

	IDT_SET_ENTRY(SYSCALL_INT, SEG_DPL_USER, syscall_int);

#ifdef SMP
	IDT_SET_ENTRY(0xff, SEG_DPL_SUPERVISOR, spurious_irq);
#endif

	MAKE_RREGISTER(idtr, &idt, IDT_NUM_ENTRIES);

	/* Load the IDT */
	__asm(
		"lidt (%%eax)\n"
	: : "a" (&idtr));

	/*
	 * Load the kernel TSS - we need this once we are going to transition between
	 * ring 0 and 3 code, as it tells the CPU where the necessary stacks are
	 * located. Note that we don't fill out the esp0 part, because this is
	 * different per-thread - md_thread_witch() will deal with it.
	 */
	memset(&kernel_tss, 0, sizeof(struct TSS));
	kernel_tss.ss0 = GDT_SEL_KERNEL_DATA;
	__asm(
		"ltr %%ax\n"
	: : "a" (GDT_SEL_KERNEL_TASK));

	bsp_pcpu.lapic_id = 0;
	bsp_pcpu.cpuid = 0;
	bsp_pcpu.tss = &kernel_tss;
	bsp_pcpu.curthread = NULL;

	/*
	 * Clear the temporary pagetable entry; this ensures we won't status with
	 * bogus mappings.
	 */
	memset(&temp_pt_entry, 0, PAGE_SIZE);

	/* Ensure the memory manager is available for action */
	mm_init();

#define PHYSMEM_MAX 10
	struct PHYSMEM {
		addr_t	address;
		size_t	length;
	} physmem[PHYSMEM_MAX];
	unsigned int physmem_num = 0;

	/*
	 * We are good to go; we now need to use the BIOS to obtain the memory map.
	 * Note that we only *obtain* the memory map and do not yet add it, because
	 * we keep switching from protected mode -> realmode -> protected mode and
	 * back, which will mess up any lower mappings since there are not
	 * adequately restored XXX
	 */
	struct realmode_regs r;
	r.ebx = 0;
	do {
		r.eax = 0xe820;
		r.edx = 0x534d4150; /* SMAP */
		r.ecx = 20;
		r.edi = realmode_get_storage_offs();
		realmode_call_int(0x15, &r);
		if (r.eax != 0x534d4150)
			break;

		#define SMAP_TYPE_MEMORY   0x01
		#define SMAP_TYPE_RESERVED 0x02
		#define SMAP_TYPE_RECLAIM  0x03
		#define SMAP_TYPE_NVS      0x04

		struct SMAP_ENTRY {
			uint32_t base_lo, base_hi;
			uint32_t len_lo, len_hi;
			uint32_t type;
		} __attribute__((packed)) *sme = (struct SMAP_ENTRY*)&realmode_store;
		/* CTASSERT(sizeof(smap_entry) == 20, "smap entry length invalid"); */

		/* Ignore any memory that is not specifically available */
		if (sme->type != SMAP_TYPE_MEMORY)
			continue;

		/* This piece of memory is available; add it */
		addr_t base = /* sme->base_hi << 32 | */ sme->base_lo;
		size_t len = /* sme->len_hi << 32 | */ sme->len_lo;

		/* Round base up to a page, and length down a page if needed */
		base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		len  &= ~(PAGE_SIZE - 1);

		physmem[physmem_num].address = base;
		physmem[physmem_num].length = len;
		physmem_num++;
	} while (r.ebx != 0);

	/* We have the chunks, feed them to the memory manager */
	for (i = 0; i < physmem_num; i++) {
		mm_zone_add(physmem[i].address, physmem[i].length);

		/*
		 * Once the first zone has been added, we can finally do away
		 * with the temp_pt hack which we need in order to get an
		 * initial pagemapping.
		 */
		if (i == 0)
			vm_init();
	}

	/* All done - it's up to the machine-independant code now */
	mi_startup();
}

void
vm_map_kernel_lowaddr(uint32_t* pd)
{
	int i;
	for (i = (addr_t)&__entry - KERNBASE; i < (addr_t)&__end - KERNBASE; i += (1 << 22)) {
		pd[i >> 22] = pd[(i + KERNBASE) >> 22];
	}
}

void
vm_unmap_kernel_lowaddr(uint32_t* pd)
{
	int i;
	for (i = (addr_t)&__entry - KERNBASE; i < (addr_t)&__end - KERNBASE; i += (1 << 22)) {
		pd[i >> 22] = 0;
	}
}

void
vm_map_kernel_addr(uint32_t* pd)
{
	addr_t i;
	for (i = (addr_t)&__entry; i < (addr_t)&__end; i += PAGE_SIZE) {
		uint32_t pd_entrynum = i >> 22;
		if (pd[pd_entrynum] == 0) {
			/* There's no directory entry yet for this item, so create one */
			addr_t new_entry = (addr_t)kmalloc(PAGE_SIZE);
			memset((void*)new_entry, 0, PAGE_SIZE);
			pd[pd_entrynum] = new_entry | PDE_P | PDE_US;
		}

		uint32_t* pt = (uint32_t*)(pd[pd_entrynum] & ~(PAGE_SIZE - 1));
		pt[(((i >> 12) & ((1 << 10) - 1)))] = (i - KERNBASE) | PTE_P | PTE_US;
	}
}

/* vim:set ts=2 sw=2: */
