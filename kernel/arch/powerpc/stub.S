.text
.globl	entry, __eabi, ofw_entry

#include <machine/vm.h>
#include "options.h"

entry:
	/* This is our entry point; we need to set up a stack */
	lis	%r1, stack@ha
	addi	%r1, %r1, stack@l

#ifdef OPTION_DEBUG_CONSOLE
	/* make sure we have a debugger console available immediately */
	bl	debugcon_init
#endif

	/* We can just run off to the md code */
	bl	md_startup

__eabi:
	/*
	 * This will be called in our main function; it's supposed to be an
	 * ABI requirement. For the moment, we do nothing here.
	 */
	blr

.data
.align	4
	.space	16384
stack:

.globl	stack
