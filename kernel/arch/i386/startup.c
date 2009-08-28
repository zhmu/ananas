#include "i386/types.h"
#include "i386/vm.h"
#include "i386/io.h"
#include "i386/macro.h"
#include "i386/realmode.h"
#include "lib.h"
#include "param.h"

/* __end is defined by the linker script and indicates the end of the kernel */
extern void* __end;

/* __entry is the entry position and the first symbol in the kernel */
extern void* __entry;

/* Pointer to our page directory */
uint32_t* pagedir;

/* Global/Interrupt descriptor tables, as preallocated by stub.s */
extern void *idt, *gdt;

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
	for (i = (addr_t)&__entry - KERNBASE; i < (addr_t)&__end - KERNBASE; i += (1 << 22)) {
		pd[i >> 22] = pd[(i + KERNBASE) >> 22];
	}

	/* It's time to... throw the switch! */
	asm(
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
	for (i = (addr_t)&__entry - KERNBASE; i < (addr_t)&__end - KERNBASE; i += (1 << 22)) {
		pd[i >> 22] = 0;
	}

	asm(
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
	GDT_SET_ENTRY32(GDT_IDX_KERNEL_CODE,   SEG_TYPE_CODE, SEG_DPL_SUPERVISOR, 0);
	GDT_SET_ENTRY32(GDT_IDX_KERNEL_DATA,   SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, 0);
	GDT_SET_ENTRY16(GDT_IDX_KERNEL_CODE16, SEG_TYPE_CODE, SEG_DPL_SUPERVISOR, stub16);
	GDT_SET_ENTRY16(GDT_IDX_KERNEL_DATA16, SEG_TYPE_DATA, SEG_DPL_SUPERVISOR, stub16);
	GDT_SET_ENTRY32(GDT_IDX_USER_CODE,     SEG_TYPE_CODE, SEG_DPL_USER,       0);
	GDT_SET_ENTRY32(GDT_IDX_USER_DATA,     SEG_TYPE_DATA, SEG_DPL_USER,       0);

	MAKE_RREGISTER(gdtr, &gdt, GDT_NUM_ENTRIES);

	/* Load the GDT, and reload our registers */
	asm(
		"lgdt (%%eax)\n"
		"mov %%bx, %%ds\n"
		"mov %%bx, %%es\n"
		"mov %%bx, %%fs\n"
		"mov %%bx, %%gs\n"
		"pushl %%ecx\n"
		"pushl $l5\n"
		"lret\n"		/* retf */
"l5:\n"
	: : "a" (&gdtr),
 	    "b" (GDT_IDX_KERNEL_DATA * 8),
	    "c" (GDT_IDX_KERNEL_CODE * 8));

	/*
	 * Prepare the IDT entries; this means mapping the exception interrupts to
	 * handlers.
	 */
	memset(&idt, 0, IDT_NUM_ENTRIES * 8);
	IDT_SET_ENTRY(0x0, 0, exception0);
	IDT_SET_ENTRY(0x1, 0, exception1);
	IDT_SET_ENTRY(0x2, 0, exception2);
	IDT_SET_ENTRY(0x3, 0, exception3);
	IDT_SET_ENTRY(0x4, 0, exception4);
	IDT_SET_ENTRY(0x5, 0, exception5);
	IDT_SET_ENTRY(0x6, 0, exception6);
	IDT_SET_ENTRY(0x7, 0, exception7);
	IDT_SET_ENTRY(0x8, 0, exception8);
	IDT_SET_ENTRY(0x9, 0, exception9);
	IDT_SET_ENTRY(0xa, 0, exceptionA);
	IDT_SET_ENTRY(0xb, 0, exceptionB);
	IDT_SET_ENTRY(0xc, 0, exceptionC);
	IDT_SET_ENTRY(0xd, 0, exceptionD);
	IDT_SET_ENTRY(0xe, 0, exceptionE);

	MAKE_RREGISTER(idtr, &idt, IDT_NUM_ENTRIES);

	/* Load the IDT */
	asm(
		"lidt (%%eax)\n"
	: : "a" (&idtr));

	/*
	 * We are good to go; we now need to use the BIOS to obtain the memory map.
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

#if 0
		uint8_t* ptr = (uint8_t*)&realmode_store;
		uint8_t* hexchars = "0123456789abcdef";
		for (i = 0; i < 20; i++) {
			uint8_t c = ptr[i];
			outb(0xe9, hexchars[c >> 4]);
			outb(0xe9, hexchars[c & 0xf]);
		}
		outb(0xe9, '\n');
#endif

		asm("cld");
		asm("cld");
		asm("cld");
		asm("cld");
	} while (r.ebx != 0);

	/* All done - it's up to the machine-independant code now */
	mi_startup();
}

/* vim:set ts=2 sw=2: */
