/*
 * Low-level initialization code.
 *
 */
.text

#include <sys/types.h>
#include "options.h"

.global __entry

__entry:
	movq	$KERNEL_STACK, %rsp

#ifdef OPTION_DEBUG_CONSOLE
	/*
	 * Make sure we have a debugger console available immediately; note that we must
	 * store %rdi as it contains the bootinfo parameter.
	 */
	pushq	%rdi
	call	debugcon_init
	popq	%rdi
#endif

	call	md_startup

	/* why are we here? */
	hlt

.align 32

	.space	0x2000
KERNEL_STACK:
