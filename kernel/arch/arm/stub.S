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

.globl _vectors_page

.align 0
_vectors_page:
	ldr	pc, _reset	/* 00: reset */
	ldr	pc, _undef	/* 04: undefined instruction */
	ldr	pc, _swi	/* 08: software interrupt */
	ldr	pc, _pre_abort	/* 0c: prefetch abort */
	ldr	pc, _data_abort	/* 10: data abort */
	nop			/* 14: reserved */
	ldr	pc, _irq	/* 18: irq */
	ldr	pc, _fiq	/* 1c: fiq */

_reset:
	.word	v_reset

_undef:
	.word	v_undef

_swi:
	.word	v_swi

_pre_abort:
	.word	v_pre_abort

_data_abort:
	.word	v_data_abort

_irq:
	.word	v_irq

_fiq:
	.word	v_fiq
