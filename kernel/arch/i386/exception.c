#include "i386/types.h"

void
exception_handler(
	uint32_t no, uint32_t ss, uint32_t gs, uint32_t fs,
	uint32_t es, uint32_t ds, uint32_t edi, uint32_t esi,
	uint32_t ebp, uint32_t esp, uint32_t ebx, uint32_t edx,
	uint32_t ecx, uint32_t eax, uint32_t errcode,
	uint32_t eip, uint32_t cs)
{
	/* TODO Handle exception... */
	while (1);
}

/* vim:set ts=2 sw=2: */
