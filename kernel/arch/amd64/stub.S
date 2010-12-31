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

	call	md_startup

#ifdef DEBUG_CONSOLE
	/* make sure we have a debugger console available immediately */
	call	debugcon_init
#endif

	/* why are we here? */
	hlt

.align 32

	.space	0x2000
KERNEL_STACK:
