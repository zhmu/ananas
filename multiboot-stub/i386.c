#include "i386.h"
#include <ananas/bootinfo.h>
#include "lib.h"

void
i386_exec(uint64_t entry, struct BOOTINFO* bootinfo)
{
	/* For now, we can just charge in to the entry point */
	uint32_t entry32 = (entry & 0x0fffffff);
	typedef void (*kentry)(int, void*, int) __attribute__((noreturn));
	((kentry)entry32)(BOOTINFO_MAGIC_1, bootinfo, BOOTINFO_MAGIC_2);
}

/* vim:set ts=2 sw=2: */
