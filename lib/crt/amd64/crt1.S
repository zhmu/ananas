/*
 * Stub called immediately after launch.
 */
/*
 * Provide a prefix for the .init/.fini sections; this is necessary to be able
 * to call them from the initialization/exit code.
 */
.section .init,"ax",@progbits
	.globl _init
	.type _init,@function
_init:

.section .fini,"ax",@progbits
	.globl _fini
	.type _fini,@function
_fini:

.text

.globl _start
.extern cstart

_start:
	/* threadinfo should be passed in %rsi, which we won't touch */
	call	cstart

	/* main finished; we shouldn't end up here since cstart() calls exit */
	hlt
