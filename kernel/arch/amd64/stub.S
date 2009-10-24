/*
 * Low-level initialization code.
 *
 */
.text

#include "types.h"

.global __entry

__entry:
	movq	$KERNEL_STACK, %rsp
	jmp	md_startup

.align 32

	.space	0x2000
KERNEL_STACK:
