#include "types.h"
#include "machine/frame.h"
#include "pcpu.h"
#include "lib.h"

void
exception_handler(struct STACKFRAME* sf)
{
	kprintf("FATAL (CPU %u): exception %u at cs:eip = %x:%x\n", PCPU_GET(cpuid), sf->sf_trapno, sf->sf_cs, sf->sf_eip);
	kprintf("eax=%x ebx=%x ecx=%x edx=%x\n", sf->sf_eax, sf->sf_ebx, sf->sf_ecx, sf->sf_edx);
	kprintf("esi=%x edi=%x ebp=%x\n", sf->sf_esi, sf->sf_edi, sf->sf_ebp);
	kprintf("ds=%x es=%x fs=%x gs=%x\n", sf->sf_ds, sf->sf_es, sf->sf_fs, sf->sf_gs);
	kprintf("ss:esp = %x:%x\n", sf->sf_ss, sf->sf_esp);
	panic("exception");
}

/* vim:set ts=2 sw=2: */
