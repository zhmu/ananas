#include "types.h"
#include "machine/bootinfo.h"
#include "machine/macro.h"
#include "machine/vm.h"
#include "vm.h"
#include "param.h"
#include "mm.h"
#include "lib.h"

/* Pointer to the next available page */
void* avail;

/* Pointer to the page directory level 4 */
uint64_t* pml4;

/* Global Descriptor Table */
uint8_t gdt[GDT_NUM_ENTRIES * 16];

/* Interrupt Descriptor Table */
uint8_t idt[IDT_NUM_ENTRIES * 16];

void*
bootstrap_get_page()
{
	void* ptr = avail;
	memset(ptr, 0, PAGE_SIZE);
	avail += PAGE_SIZE;
	return ptr;
}

void
md_startup(struct BOOTINFO* bi)
{
	/*
	 * This function will be called by the trampoline code, and hands us a bootinfo
	 * structure which contains the E820 memory map. We use this data to bootstrap
	 * the VM and initialize the memory manager.
	 */
	if (bi->bi_ident != BI_IDENT)
		panic("going nowhere without my bootinfo!");

	/*
	 * First of all, initialize a new Global Descriptor Table; the trampoline
	 * adds a lot of now-useless fields which we can do without, and we'll need
	 * things like a TSS.
	 */
	memset(gdt, 0, sizeof(gdt));
	GDT_SET_CODE64(gdt, GDT_IDX_KERNEL_CODE, SEG_DPL_SUPERVISOR);
	GDT_SET_DATA64(gdt, GDT_IDX_KERNEL_DATA);
	GDT_SET_CODE64(gdt, GDT_IDX_USER_CODE, SEG_DPL_USER);
	GDT_SET_DATA64(gdt, GDT_IDX_USER_DATA);
	MAKE_RREGISTER(gdtr, gdt, GDT_NUM_ENTRIES);

	/* Load the GDT, and reload our registers */
	__asm(
		"lgdt (%%rax)\n"
		"mov %%bx, %%ds\n"
		"mov %%bx, %%es\n"
		/*
		 * All we need is 'mov %%cx, %%cs'; For the first time, directly editing
		 * CS is useful, so it can't be used :-)
		 */
		"pushw	%%cx\n"
		"pushq	$l1\n"
		"retq\n"
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
	IDT_SET_ENTRY(idt, 17, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception16);
	IDT_SET_ENTRY(idt, 18, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception18);
	IDT_SET_ENTRY(idt, 19, SEG_DPL_SUPERVISOR, GDT_SEL_KERNEL_CODE, exception19);

	/* Load the IDT */
	MAKE_RREGISTER(idtr, idt, IDT_NUM_ENTRIES);
	__asm(
		"lidt (%%rax)\n"
	: : "a" (&idtr));

	/*
	 * The loader tells us how large the kernel is; we use pages directly after
	 * the kernel.
	 */
	avail = (void*)bi->bi_kern_end;
	pml4 = (uint64_t*)bootstrap_get_page();

	/* First of all, map the kernel; we won't get far without it */
	void *__entry, *__end;
	addr_t from = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t   to = (addr_t)&__end;
	to += PAGE_SIZE - to % PAGE_SIZE;
	
	vm_mapto(from, from & 0x0fffffff /* HACK */, 32);

	/* Map the bootinfo block */
	vm_map((addr_t)bi, 1);
	vm_map((addr_t)bi->bi_e820_addr, 1);

	__asm(
		/* Set CR3 to the physical address of the page directory */
		"movq %%rax, %%cr3\n"
	: : "a" ((addr_t)pml4 & 0x0fffffff /* HACK */));

	#define SMAP_TYPE_MEMORY   0x01
	#define SMAP_TYPE_RESERVED 0x02
	#define SMAP_TYPE_RECLAIM  0x03
	#define SMAP_TYPE_NVS      0x04
	#define SMAP_TYPE_FINAL    0xFFFFFFFF /* added by the bootloader */

	struct SMAP_ENTRY {
		uint32_t base_lo, base_hi;
		uint32_t len_lo, len_hi;
		uint32_t type;
	} __attribute__((packed));

	/* Walk the memory map, and add each item one by one */
	struct SMAP_ENTRY* sme;
	for (sme = (struct SMAP_ENTRY*)(addr_t)bi->bi_e820_addr; sme->type != SMAP_TYPE_FINAL; sme++) {
		/* Ignore any memory that is not specifically available */
		if (sme->type != SMAP_TYPE_MEMORY)
			continue;

		addr_t base = (addr_t)sme->base_hi << 32 | sme->base_lo;
		size_t len = (size_t)sme->len_hi << 32 | sme->len_lo;

		base  = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
		len  &= ~(PAGE_SIZE - 1);
		mm_zone_add(base, len);
	}

	mi_startup();
}

/* vim:set ts=2 sw=2: */
