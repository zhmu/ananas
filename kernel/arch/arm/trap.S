.text
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
	.word	exc_reset

_undef:
	.word	exc_undef

_swi:
	.word	exc_swi

_pre_abort:
	.word	exc_pre_abort

_data_abort:
	.word	exc_data_abort

_irq:
	.word	exc_irq

_fiq:
	.word	exc_fiq
