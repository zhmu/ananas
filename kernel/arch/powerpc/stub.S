.text
.globl	entry, __eabi, ofw_entry

entry:
	/* XXX we trust the loader stack for nwo */

	/* We can just run off to the md code */
	b	md_startup

__eabi:
	/*
	 * This will be called in our main function; it's supposed to be an
	 * ABI requirement. For the moment, we do nothing here.
	 */
	blr
