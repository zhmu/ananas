#include "i386/types.h"
#include "i386/macro.h"
#include "i386/vm.h"
#include "i386/realmode.h"
#include "lib.h"
#include "param.h"

extern void *__end, *__entry;
extern uint32_t* pagedir;
extern void *gdt, *idt;

extern uint8_t* realmode_int;
extern uint8_t* realmode_regs;
extern uint8_t* realmode_store;

void
realmode_call_int(unsigned char num, struct realmode_regs* r)
{
	uint32_t i;

	/*
	 * Set up the interrupt number; this is just patched in the code.
	 */
	*(uint8_t*)&realmode_int = num;
	memcpy(&realmode_regs, r, sizeof(struct realmode_regs));

	/*
	 * According to Intel's IA-32 Development Manual Volume 3: System Programming
	 * Guide (page 9-15) we need to perform the following actions:
	 *
	 * 1) Disable interrupts
	 * 2) If paging is enabled:
	 *    - Transfer program control to a linear address that is identity mapped
	 *    - Ensure that GDT/IDT are identity mapped
	 *    - Clear PG bit in CR0
	 *    - Place 0x0 in CR3 to flush TLB
	 * 3) Transfer control to a readable segment with a limit of 64KB
	 * 4) Load SS, DS, ES, FS and GS with a selector with a 64KB limit
	 * 5) Execute LIDT to point to a real-address mode address interrupt table.
	 * 6) Clear PE flag in CR0
	 * 7) Execute far JMP to real-address mode
	 * 8) Load SS, DS, ES, FS, GS as needed by real-mode code.
	 * 9) Enable interrupts
	 *
	 * Paging is always enabled, so we'll need to consider (2) as well.
	 */

	/*
	 * First of all, we need to set up an identity mapped address for the IDT/GDT
	 * and this function. The easiest way to do this, is to just re-map the entire
	 * kernel space back to where our bootloader left us.
	 */
	for (i = (addr_t)&__entry - KERNBASE; i < (addr_t)&__end - KERNBASE; i += (1 << 22)) {
		pagedir[i >> 22] = pagedir[(i + KERNBASE) >> 22];
	}

	/* (1) Transfer control to a linear address that is identity mapped */
	asm (
		"movl	$l1, %%eax\n"
		"subl	%%ebx, %%eax\n"
		"jmp *%%eax\n"
		"l1:\n"
	: : "b" (KERNBASE));

	/* Ensure GDT/IDT are identity mapped and activate 'em */
	addr_t lowGDT = (addr_t)&gdt - KERNBASE;
	addr_t lowIDT = (addr_t)0;
	MAKE_RREGISTER(low_gdtr, lowGDT, GDT_NUM_ENTRIES);
	MAKE_RREGISTER(low_idtr, lowIDT, 0);
	asm (
		"lidt (%%eax)\n"
		"lgdt (%%ebx)\n"
	: : "a" (&low_idtr),
	    "b" (&low_gdtr));
	asm(
		/* store the entry point of the return call */
		"pushl	%%eax\n"
		"pushl	$l2\n"
		/* jump to the realmode stub */
		"pushl	%%ebx\n"
		"pushl	$0\n"
		"lret\n"
"l2:\n"
	 : : "a" (GDT_IDX_KERNEL_CODE   * 8),
	     "b" (GDT_IDX_KERNEL_CODE16 * 8));

	/*
	 * Control is now in the hands of 'realmode16_stub'. Once it is done,
	 * it will do a lretw and end up here...
	 */

	/* OK, we are done; restore the GDT/IDT */
	MAKE_RREGISTER(gdtr, &gdt, GDT_NUM_ENTRIES);
	MAKE_RREGISTER(idtr, &idt, IDT_NUM_ENTRIES);
	asm (
		"lidt (%%eax)\n"
		"lgdt (%%ebx)\n"
	: : "a" (&idtr),
	    "b" (&gdtr));

	/* Throw away the mappings */
	for (i = (addr_t)&__entry - KERNBASE; i < (addr_t)&__end - KERNBASE; i += (1 << 22)) {
		pagedir[i >> 22] = 0;
	}

	/* Last but not least, copy the register contents back */
	memcpy(r, &realmode_regs, sizeof(struct realmode_regs));
}

/* vim:set ts=2 sw=2: */
