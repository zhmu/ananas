.text
.globl	entry, __eabi, ofw_entry

entry:
	/* This is our entry point; we need to set up a stack */
	lis	1, stack@ha
	addi	1, 1, stack@l

	/* r5 contains the OFW entry point, so store it */
	lis	11, ofw_entry@ha
	stw	5, ofw_entry@l(11)

	/* Go to our platform code */
	b	platform_init

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

ofw_entry:
	.long	0

	/*
	 * XXX for some reason, the 4 bytes after here are corrupted; best
	 * place a dummy here to avoid problems... XXX
	 */
	.long	0
