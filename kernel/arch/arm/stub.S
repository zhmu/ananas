.text
.globl	__entry
.extern md_startup

#include <ananas/arm/param.h>
#include <ananas/arm/vm.h>

__entry:
	/* Switch to our own stack */
	adr	sp, kstack

	/* Disable the MMU and caches */
	mrc	p15, 0, r1, c1, c0, 0
	bic	r1, r1, #(CP15_CTRL_MMU | CP15_CTRL_WRITEBUFFER) 
	bic	r1, r1, #(CP15_CTRL_ICACHE)
	mcr	p15, 0, r1, c1, c0, 0

	/* Jump to the startup code */
	ldr	r1, startup
	mov	r2, #KERNBASE
	sub	r1, r1, r2
	mov	pc, r1


startup:
	.word	md_startup

	.space	2048
kstack:
