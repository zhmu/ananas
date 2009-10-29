/*
 * Low-level initialization code.
 *
 */
.text

#include "types.h"

.global __entry

__entry:
	movq	$KERNEL_STACK, %rsp

	call	md_startup

	/* why are we here? */
	hlt

.align 32

	.space	0x2000
KERNEL_STACK:
