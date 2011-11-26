.text
.code32
.globl entry

entry:
	xchgw	%bx,%bx

	pushl	%ebp
	movl	%esp, %ebp

	/* Fetch ImageHandle and SystemTable arguments for later */
	pushl 12(%ebp)
	pushl  8(%ebp)

	/* Figure out our relocation base address as %esi */
	call	1f
1:	popl	%esi
	subl	$1b, %esi

	/*
	 * Calculate our relocated base address as %esi and the address of our
	 * dynamic symbol table, _DYNAMIC (it is illustrated in Figure 2-6 of
	 * the ELF specification)
	 */
	movl	$_DYNAMIC, %eax
	addl	%esi, %eax
	addl	$ELFBASE, %esi

	/* Call efi_relocate(dynamic, elf_base); */
	pushl	%esi
	pushl	%eax
	call	efi_relocate
	addl	$8, %esp
  
	/* All done; run the EFI code with our arguments as stored previously */
  	call	EfiEntry

	popl	%ebp
	ret

.data

.section .reloc
 	.long	0
 	.long	10
 	.word	0
