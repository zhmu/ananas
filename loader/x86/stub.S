.text
.code16
.globl	entry, realcall
.globl	rm_regs, rm_buffer

#include "param.h"

entry:
	/*
	 * First of all, I *hate* *hate* *hate* x86 - it's a legacy-filled
	 * platform with so many annoying implementation details that it
	 * isn't even funny.
	 *
	 * For the moment, let's assume someone loads this code at a
	 * reasonable location (CODE_BASE). Then we have to reallocate our
	 * code to where we are linked (REALLOC_BASE) and jump to protected
	 * mode,
	 *
	 * Strictly speaking, this entire hunk of code can function easily
	 * in unreal mode and move everything it loads to the correct location.
	 * However, this involves overly complicating our toolchain as it
	 * cannot generate 16 bit code. Thus, we just keep switching back and
	 * forth between real- and protected mode... :-/
	 *
	 * I'd expect no other platforms being this retarded (realmode should
	 * have been shot a long time ago - but as no one seemed to care enough
	 * to correctly implement I/O functions that work in protected mode...)
	 */
	cli
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	$(rm_stack - entry), %sp

	/*
	 * Reallocate our code to what we were built for.
	 */
	movw	$(REALLOC_BASE >> 4), %ax
	movw	%ax, %es
	movw	$CODE_BASE, %si
	xorw	%di, %di
	movw	$8192, %cx		/* 32KB (64 sectors) */
	rep	movsd

	/*
	 * Before we can jump to protected mode, we first have to enable the
	 * A20 line (which is a horrible hack to ensure compatibility back in
	 * the 8086 days and should have been nuked a long time ago...).
	 */
	call	enable_a20

	/*
	 * It's time to jump to big bad protected mode. First of all, we
	 * must load the GDT. Then we can jump to pmode.
	 */
	lgdt	(gdtr - entry + CODE_BASE)

	/* Flip the protected mode bit */
	movl	%cr0, %eax
	orl	$1, %eax
	movl	%eax, %cr0

	/* Go to 32 bit pmode */
	.byte	0x66
	.byte	0xea
	.long	(REALLOC_BASE + code32 - entry)
	.word	0x8

enable_a20:
	call	wait_kbd
	movb	$0xd1, %al	/* command write */
	outb	%al, $0x64

	call	wait_kbd
	movb	$0xdf, %al	/* enable A20 */
	outb	%al, $0x60

	jmp	wait_kbd

wait_kbd:
	/*
	 * We time out after 65536 cycles; a keyboard may not
	 * exist anymore.
	 */
	xorw	%cx, %cx

wait_kbd_1:
	dec	%cx
	jz	wait_kbd_2

	inb	$0x64, %al
	test	$2, %al		/* bit 1: input buffer full */
	jnz	wait_kbd_1

wait_kbd_2:
	ret

.code32

code32:
	/* Fix up our segments and stack */
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movl	(x86_pool_pointer - entry + REALLOC_BASE), %esp
	addl	$STACK_SIZE, %esp
	addl	$STACK_SIZE, (x86_pool_pointer - entry + REALLOC_BASE)

	/* Call our main function - this shouldn't return */
	call	main

gdtr:	.word	(gdt_end - gdt)
	.long	(gdt - entry + CODE_BASE)

.align	16

gdt:	.long	0			/*  0: null descriptor */
	.long	0

	.long	0x0000ffff		/*  8: 32-bit code */
	.long	0x00cf9800

	.long	0x0000ffff		/* 10: 32-bit data */
	.long	0x00cf9200

	.word	0xffff			/* 18: 16-bit code */
	.word	REALLOC_BASE & 0xffff
	.long	0x00009800

	.word	0xffff			/* 20: 16-bit data */
	.word	REALLOC_BASE & 0xffff
	.long	0x00009200

gdt_end:

realcall:
	/*
	 * This function is responsible for calling a realmode BIOS function.
	 * All these mode-switches mean it will be slow, but at least this will
	 * work everywhere (plus, this is the easiest to follow - an
	 * alternative would be so hack up the entire loader to run in 16 bit
	 * unreal mode, but that'd just be nasty - how do you get GCC to generate
	 * 16 bit code anyway these days?))
	 */

	/*
	 * We must store %esp (which we have to do in a temporary register as it will
	 * get nuked. Note that we also have to save %ebp as the compiler expects this.
	 */
	pushl	%ebp
	movl	%esp, realcall_esp

	/*
	 * Go to 16 bit pmode - you can't go from 32 bit pmode -> 16 bit realmode in a
	 * single go.
	 */
	.byte	0xea
	.long	realcall16_pm - entry
	.word	0x18

realcall16_pm:
	/* Get out of pmode... */
	movl	%cr0, %eax
	andl	$~1, %eax
	movl	%eax, %cr0

	/* ...and force a jump to realmode */
	.byte	0xea
	.word	realcall16 - entry
	.word	REALLOC_BASE >> 4

.code16

realcall16:
	/*
	 * OK, we are in 16 bit realmode now. Set up a stack, restore the
	 * registers and call the freakin' interrupt!
	 */
	movw	%cs, %ax
	movw	%ax, %ss
	movw	%ax, %ds
	movw	%ax, %es
	movw	$(rm_stack - entry), %sp

	/* Patch the interrupt number. Hey, this is realmode - anything goes! */
	movb	(r_int - entry), %al
	movb	%al, (rm_int - entry)

	/* Restore the flags */
	movl	(r_flags - entry), %eax
	pushl	%eax
	popfl

	/* Restore the registers */
	movl	(r_eax - entry), %eax
	movl	(r_ebx - entry), %ebx
	movl	(r_ecx - entry), %ecx
	movl	(r_edx - entry), %edx
	movl	(r_ebp - entry), %ebp
	movl	(r_esi - entry), %esi
	movl	(r_edi - entry), %edi

	.byte	0xcd
rm_int:	.byte	0x03

	/* Save the registers */
	movl	%eax, (r_eax - entry)
	movl	%ebx, (r_ebx - entry)
	movl	%ecx, (r_ecx - entry)
	movl	%edx, (r_edx - entry)
	movl	%ebp, (r_ebp - entry)
	movl	%esi, (r_esi - entry)
	movl	%edi, (r_edi - entry)

	/* Store our flags */
	pushfl
	popl	%eax
	movl	%eax, (r_flags - entry)

	/* Ensure interrupts are not set - this would hurt us */
	cli

	/*
	 * That was fun. Now, we need to return to protected mode!
	 */
	movl	%cr0, %eax
	orl	$1, %eax
	movl	%eax, %cr0

	/* Go to 32 bit pmode */
	.byte	0x66
	.byte	0xea
	.long	(REALLOC_BASE + rc32_return - entry)
	.word	0x8

.code32
rc32_return:
	/*
	 * We are back and still breathing :-) Now, we must restore our segment
	 * registers and %esp. We needn't worry about our ordinary registers
	 * because these will be tucked safely in r_xxx.
	 */
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movl	realcall_esp, %esp
	popl	%ebp
	ret

rm_regs:
r_eax:	.long	0
r_ebx:	.long	0
r_ecx:	.long	0
r_edx:	.long	0
r_ebp:	.long	0
r_esi:	.long	0
r_edi:	.long	0
r_flags:.long	0
r_int:	.byte	0
	
realcall_esp:	.long	0

/*
 * We need a realmode buffer; this is used to read sectors etc.
 */
rm_buffer:
	.space	512

/*
 * Stack used for realmode work. This must be kept here since
 * it must exist in the same segment as the realmode code.
 */
	.space	1024
rm_stack:

