#include "types.h"
#include "machine/bootinfo.h"
#include "machine/macro.h"
#include "machine/vm.h"
#include "machine/pcpu.h"
#include "machine/thread.h"
#include "i386/io.h"			/* XXX for PIC I/O */
#include "vm.h"
#include "param.h"
#include "pcpu.h"
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

/* Boot CPU pcpu structure */
static struct PCPU bsp_pcpu;

/* TSS used by the kernel */
struct TSS kernel_tss;

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
	iop
		 * CS is useful, so it can't be used :-)
		 */
		"pushw	%%cx\n"
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
	 * Remap the interrupts; by default, IRQ 0-7 are mapped to interrupts 0x08 - 0x0f
	 * and IRQ 8-15 to 0x70 - 0x77. We remap IRQ 0-15 to 0x20-0x2f (since 0..0x1f is
	 * reserved by Intel).
	 *
	 * XXX this is duplicated in i386/startup.c - best make it uniform
	 */
#define IO_WAIT() do { int i = 10; while (i--) /* NOTHING */ ; } while (0);

	unsigned int mask1 = inb(PIC1_DATA);
	unsigned int mask2 = inb(PIC2_DATA);
	/* Start initialization: the PIC will wait for 3 command bta ytes */
	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); IO_WAIT();
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); IO_WAIT();
	/* Data byte 1 is the interrupt vector offset - program for interrupt 0x20-0x2f */
	outb(PIC1_DATA, 0x20); IO_WAIT();
	outb(PIC2_DATA, 0x28); IO_WAIT();
	/* Data byte 2 tells the PIC how they are wired */
	outb(PIC1_DATA, 0x04); IO_WAIT();
	outb(PIC2_DATA, 0x02); IO_WAIT();
	/* Data byte 3 contains environment flags */
	outb(PIC1_DATA, ICW4_8086); IO_WAIT();
	outb(PIC2_DATA, ICW4_8086); IO_WAIT();
	/* Restore PIC masks */
	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);

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
	
	/* Now that we can use the PCPU data, initialize it */
	memset(&bsp_pcpu, 0, sizeof(struct PCPU));

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
	avail = (void*)bi->bi_kern_end;
	pml4 = (uint64_t*)bootstrap_get_page();

	/* First of all, map the kernel; we won't get far without it */
	void *__entry, *__end;
	addr_t from = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t   to = (addr_t)&__end;
	to += PAGE_SIZE - to % PAGE_SIZE;

	kprintf("map length = 0x%x bytes\n", to - from);
	vm_mapto(from, from & 0x0fffffff /* HACK */, 64);

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
	vm_init();

	mi_startup();
}

void
vm_map_kernel_addr(void* pml4)
{
	void *__entry, *__end;
	addr_t from = (addr_t)&__entry & ~(PAGE_SIZE - 1);
	addr_t   to = (addr_t)&__end;
	to += PAGE_SIZE - to % PAGE_SIZE;
	
	vm_mapto_pagedir(pml4, from, from & 0x0fffffff /* HACK */, 32, 1);
}

/* vim:set ts=2 sw=2: */
