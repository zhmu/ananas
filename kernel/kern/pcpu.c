#include "mm.h"
#include "machine/pcpu.h"
#include "pcpu.h"

struct PCPU*
pcpu_init()
{
	struct PCPU* pcpu = kmalloc(sizeof(struct PCPU));

	return pcpu;
}

/* vim:set ts=2 sw=2: */
