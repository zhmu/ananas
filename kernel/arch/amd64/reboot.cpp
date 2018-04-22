#include <ananas/types.h>
#include "kernel/lib.h"
#include "kernel-md/md.h"

namespace md {

void
Reboot()
{
	/*
	 * We attempt to trigger a reboot by clearing the IDT and forcing an
	 * interrupt - this should get us from fault -> double fault -> triple
	 * fault which generally results in a cold restart (but it doesn't work
	 * on all architectures...)
	 */
	uint8_t idtr[10];
	memset(idtr, 0, sizeof(idtr));
	__asm __volatile(
		"cli\n"
		"lidt (%%rax)\n"
		"int $3\n"
 	: : "a" (&idtr));
	for(;;) /* nothing, in case we survive */ ;
	/* NOTREACHED */
}

} // namespace md

/* vim:set ts=2 sw=2: */
