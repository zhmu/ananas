.text
.align	4
.globl	entry
.type	entry,@function

entry:
	xorq	%rbp,%rbp

	/*
	 * Invoke the dynamic linker; we expect the stack to be laid out as
	 * stated in Figure 3.9 of the AMD64 ELF ABI.
	 */
	movq	%rsp, %rdi
	subq	$8, %rsp
	movq	%rsp, %rsi	/* pointer to cleanup address */
	call	rtld

	/*
	 * We expect to end up in crt0-common.c/__start which will register
	 * the cleanup pointer for us - take the value from the stack.
	 */
	popq	%rsi
	jmp	*%rax

	.globl	rtld_bind_trampoline
	.type		rtld_bind_trampoline,@function

rtld_bind_trampoline:
	nop

	/*
	 * We are entered with [object] and [reloff] on the stack, the latter is the
	 * byte offset of the relocation entry in the PLT.
	 *
	 * We save what is needed by the ABI. Note that we are built with no
	 * SSE instructions, so we needn't save those.
	 *
	 * Stack layout is the following:
	 *
	 * obj		0x48
	 * reloff	0x40
	 * %rax		0x38
	 * %rsi		0x30
	 * %rdi		0x28
	 * %rbx		0x20
	 * %rcx		0x18
	 * %rdx		0x10
	 * %r8		0x08
	 * %r9		0x00
	 */
	pushq		%rax
	pushq		%rsi
	pushq		%rdi
	pushq		%rbx
	pushq		%rcx
	pushq		%rdx
	pushq		%r8
	pushq		%r9

	/* Grab arguments from stack and invoke rtld_bind() */
	movq		0x40(%rsp),%rdi	/* obj */
	movq		0x48(%rsp),%rsi	/* reloff */
	call		rtld_bind

	/*
	 * Resulting address is in %rax; overwrite reloff with the address so we can
	 * return to it without kludging registers
	 */
	movq		%rax,0x48(%rsp)

	popq		%r9
	popq		%r8
	popq		%rdx
	popq		%rcx
	popq		%rbx
	popq		%rdi
	popq		%rsi
	popq		%rax

	/* Discard obj; reloff is overwritten with address so we ret to it */
	addq		$8,%rsp
	ret

#
# Helper function to obtain the address of _DYNAMIC - needed for lld (see lld
# bug 33802)
#
.globl	rtld_dynamic
.type	rtld_dynamic,@function
rtld_dynamic:
.weak	_DYNAMIC
.hidden	_DYNAMIC
	lea	_DYNAMIC(%rip),%rax
	ret
