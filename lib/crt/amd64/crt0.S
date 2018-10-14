.text

.globl	_start
.type	_start,@function
_start:
	/* First argument is the stack pointer; we follow the ELF ABI spec */
	movq	%rsp, %rdi

	/*
	 * We need to force stack alignment to 16 bytes, otherwise SSE
	 * instructions will not work correctly.
	 */
	andq	$~15, %rsp

	/*
	 * However, we know that __start() will create a stack frame, which
	 * causes it to misalign %rsp by 8 so cope with that here.
	 */
	subq	$8, %rsp
	jmp	__start
