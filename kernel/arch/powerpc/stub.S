.text
.globl	entry, __eabi, ofw_entry

entry:
	/* This is our entry point; we need to set up a stack */
	lis	1, stack@ha
	addi	1, 1, stack@l

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
