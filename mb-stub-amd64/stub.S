.text

.globl __start, amd64_launch

#define STACK_SIZE 4096

__start:
		jmp		entry

.align 8

/* boot flags: need 4kb alignment, memory info. provide offsets */
#define	BOOT_FLAGS ((1 << 0) | (1 << 1) | (1 << 16))

header:
	.long	0x1badb002	/* magic */
	.long	BOOT_FLAGS	/* flags */
	.long	-(0x1badb002 + BOOT_FLAGS)	/* checksum */
	.long	header		/* header address */
	.long	__start		/* load addr */
	.long	0		/* load end address (everything) */
	.long	0		/* bss end address */
	.long	__start		/* entry address */

entry:
	/* Obtain some stack we can use for the time being */
	movl	$0x7c00, %esp

	/* Ensure we have sane flags */
	pushl	$0
	popfl

	/* Ensure we use the correct GDT */
	lgdt	%cs:(gdtr)

	/*
	 * Check if this is a valid multiboot signature; if not,
	 * clear the multiboot info structure so that startup()
	 * will complain.
	 */
	cmpl	$0x2badb002, %eax
	je	1f

	xorl	%ebx, %ebx

1:	/* Call our C code */
	pushl	%ebx		/* multiboot info structure */
	call	startup

	/* why are we here? */
	hlt

gdtr:	.word	(gdt_end - gdt)
	.long	gdt

.align	16

gdt:	.long	0			/*  0: null descriptor */
	.long	0

	.long	0x0000ffff		/*  8: 32-bit code */
	.long	0x00cf9800

	.long	0x0000ffff		/* 10: 32-bit data */
	.long	0x00cf9200

	.long	0         		/* 18: 64-bit code */
	.long	0x00209800

	.long	0         		/* 20: 64-bit data */
	.long	0x00009200

gdt_end:


/* Control Register flags */
#define CR0_PGE (1 << 31)
#define CR4_PAE	(1 << 5)

/* Extended Feature Register and flags */
#define MSR_EFER	0xc0000080
#define EFER_SCE	(1 << 0)
#define EFER_LME	(1 << 8)

amd64_launch:
	/*
	 * We are called with three arguments:
	 * - uint64_t entry: entry point to execute
	 * - void* pml4: 64-bit page table root
	 * - void* bootinfo: bootinfo to supply to the kernel 
	 */
	movl	16(%esp), %ebp		/* bootinfo */
	movl	12(%esp), %ebx		/* pml4 */
	movl	 8(%esp), %esi		/* entry, hi */
	movl	 4(%esp), %edi		/* entry, lo */

	/* Enable PAE */
	movl	%cr4, %eax
	orl	$CR4_PAE, %eax
	movl	%eax, %cr4

	/* Enable paging table */
	movl	%ebx, %cr3

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
	ljmp	$0x18, $entry64

.code64

entry64:
	/* Throw away upper 32 bits of %rsi and %rdi */
	movl	%esi, %esi
	movl	%edi, %edi

	/* Construct entry point %ebx = (%rsi << 32) | %rdi */
	movq	%rsi, %rbx
	shlq	$32, %rbx
	addq	%rdi, %rbx

	/* Obtain bootinfo structure; this is passed to the kernel as %rdi */
	movq	%rbp, %rdi
	xchg	%edi, %edi

	/* Set up the base segment registers for the kernel */
	movq	$0x20, %rax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	/* Transfer control to the ELF file we loaded */
	jmp	*%rbx

	/* Why are we here? */
	hlt
