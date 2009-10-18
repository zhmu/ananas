#include "i386/types.h"
#include "pcpu.h"
#include "lib.h"

void
exception_handler(
	uint32_t no, uint32_t ss, uint32_t gs, uint32_t fs,
	uint32_t es, uint32_t ds, uint32_t edi, uint32_t esi,
	uint32_t ebp, uint32_t esp, uint32_t ebx, uint32_t edx,
	uint32_t ecx, uint32_t eax, uint32_t errcode,
	uint32_t eip, uint32_t cs)
{
	kprintf("FATAL (CPU %u): exception %u at cs:eip = %x:%x\n", PCPU_GET(cpuid), no, cs, eip);
	kprintf("eax=%x ebx=%x ecx=%x edx=%x\n", eax, ebx, ecx, edx);
	kprintf("esi=%x edi=%x ebp=%x\n", esi, edi, ebp);
	kprintf("ds=%x es=%x fs=%x gs=%x\n", ds, es, fs, gs);
	kprintf("ss:esp = %x:%x\n", ss, esp);

	panic("exception");
}

/* vim:set ts=2 sw=2: */
