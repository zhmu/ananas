.text
.code32
.globl	x86_64_launch

#ifdef X86_64

/* Control Register flags */
#define CR0_PGE (1 << 31)
#define CR4_PAE	(1 << 5)

/* Extended Feature Register and flags */
#define MSR_EFER	0xc0000080
#define EFER_SCE	(1 << 0)
#define EFER_LME	(1 << 8)

x86_64_launch:
	/*
	 * We are called with two arguments:
	 * - uint64_t entry: entry point to execute
	 * - void* bootinfo: bootinfo to supply to the application
	 */
	movl	12(%esp), %ebp		/* bootinfo */
	movl	 8(%esp), %esi		/* entry, hi */
	movl	 4(%esp), %edi		/* entry, lo */

	/* Enable PAE */
	movl	%cr4, %eax
	orl	$CR4_PAE, %eax
	movl	%eax, %cr4

	/* Enable paging table */
	movl	(pml4), %eax
	movl	%eax, %cr3

	/* Enable Long Mode */
	movl	$MSR_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Enable paging */
	movl	%cr0, %eax
	orl	$CR0_PGE, %eax
	movl	%eax, %cr0
	
	/* Jump to our 64 bit code */
	ljmp	$0x28, $entry64

.code64

entry64:
	/* Throw away upper 32 bits of %rsi and %rdi */
	xchg	%esi, %esi
	xchg	%edi, %edi

	/* Construct entry point %ebx = (%rsi << 32) | %rdi */
	movq	%rsi, %rbx
	shlq	$32, %rbx
	addq	%rdi, %rbx

	/* Obtain bootinfo structure; this is passed to the kernel as %rdi */
	movq	%rbp, %rdi
	xchg	%edi, %edi

	/* Set up the base segment registers for the kernel */
	movq	$0x30, %rax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	/* Transfer control to the ELF file we loaded */
	jmp	*%rbx

	/* Why are we here? */
	hlt

#endif /* X86_64 */
