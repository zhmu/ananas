.text
.code16
.globl	entry, realcall
.globl	rm_stack, rm_regs, rm_buffer
#ifdef X86_64
.globl	cpu_64bit_capable
#endif

#include "param.h"

#define EFLAGS_CPUID	(1 << 21)
#define CPUID_LM	(1 << 29)

entry:
	/*
	 * First of all, I *hate* *hate* *hate* x86 - it's a legacy-filled
	 * platform with so many annoying implementation details that it
	 * isn't even funny.
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
	cld
	xorw	%ax, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	$(rm_stack), %sp

	/*
	 * Setup a serial port if wanted, to aid in debugging.
	 */
#ifdef DEBUG_SERIAL
	/* disable serial interrupts */
	mov     $0x3f8+1,%dx
	mov     $0x00, %al
	outb    %al, %dx

	/* enable DLAB (sets baud rate divisor) */
	mov     $0x3f8+3,%dx
	mov     $0x80, %al
	outb    %al, %dx

	/* set divisor low byte for 9600 baud */
	mov     $0x3f8+0,%dx
	mov     $0x0c, %al
	outb    %al, %dx

	/* set divisor hi byte for 9600 baud */
	mov     $0x3f8+1,%dx
	mov     $0x0, %al
	outb    %al, %dx

	/* 8 bits, no parity and 1 stop bit */
	mov     $0x3f8+3,%dx
	mov     $0x3, %al
	outb    %al, %dx

	/* enable FIFO, clear it and set it for a 14 byte threshold */
	mov     $0x3f8+2,%dx
	mov     $0xc7, %al
	outb    %al, %dx
#endif

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
	lgdt	(gdtr)

	/* Flip the protected mode bit */
	movl	%cr0, %eax
	orl	$1, %eax
	movl	%eax, %cr0

	/* Go to 32 bit pmode */
	.byte	0x66
	.byte	0xea
	.long	code32
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
	addl	$STACK_SIZE, x86_pool_pointer
	movl	x86_pool_pointer, %esp

#ifdef X86_64
	/*
	 * See if this CPU is 64 bit (so-called Long Mode) capable; this allows us
	 * to have the loader reject 64 bit kernels if they are not supported
	 * instead of just rebooting.
	 *
	 * The following code is based on AMD64 Architecture Programmers Manual
	 * Volume 3: General Purpose and System Functions, CPUID instruction.
	 */
	pushfl
	pop	%eax
	movl	%eax, %ebx			/* store original flags */
	xorl	$EFLAGS_CPUID, %eax		/* try to toggle CPUID bit */
	pushl	%eax
	popfl

	pushfl
	popl	%eax				/* get flags; if the bit changed... */
	cmp	%eax, %ebx			/* ... we do not support cpuid... */
	je	not_lm_capable			/* ... and thus no long mode either */

	movl	$0x80000001, %eax		/* Ask CPU for extended flags */
	cpuid
	andl	$CPUID_LM, %edx
	orl	%edx, %edx
	jz	not_lm_capable

	incl	cpu_64bit_capable

not_lm_capable:
#endif /* X86_64 */

	/* Call our main function - this shouldn't return */
	call	main

gdtr:	.word	(gdt_end - gdt)
	.long	gdt

.align	16

gdt:	.long	0			/*  0: null descriptor */
	.long	0

	.long	0x0000ffff		/*  8: 32-bit code */
	.long	0x00cf9800

	.long	0x0000ffff		/* 10: 32-bit data */
	.long	0x00cf9200

	.word	0xffff			/* 18: 16-bit code */
	.word	CODE_BASE & 0xffff
	.long	0x00009800

	.word	0xffff			/* 20: 16-bit data */
	.word	CODE_BASE & 0xffff
	.long	0x00009200

	.long	0         		/* 28: 64-bit code */
	.long	0x00209800

	.long	0         		/* 30: 64-bit data */
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
	.word	realcall16
	.word	0

.code16

realcall16:
	/*
	 * OK, we are in 16 bit realmode now. Set up a stack, restore the
	 * registers and call the freakin' interrupt! Note that we
	 * are called using a far direct call, so we can assume %cs = 0. Thus,
	 * if we index with %cs, we are certain that we can use direct offsets.
	 */

	/* Patch the interrupt number and call pointer. Hey, this is realmode - anything goes! */
	movb	%cs:(r_int), %al
	movb	%al, %cs:(rm_int)
	movw	%cs:(r_cs), %ax
	movw	%ax, %cs:(rm_cs)
	movw	%cs:(r_ip), %ax
	movw	%ax, %cs:(rm_ip)

	/* Update ds/es/ss segments */
	movw	%cs:(r_ss), %ax
	movw	%ax, %ss
	movw	%cs:(r_es), %ax
	movw	%ax, %es
	movw	%cs:(r_ds), %ax
	movw	%ax, %ds

	/* Restore the flags */
	movl	%cs:(r_flags), %eax
	pushl	%eax
	popfl

	/* Restore the registers */
	movl	%cs:(r_eax), %eax
	movl	%cs:(r_ebx), %ebx
	movl	%cs:(r_ecx), %ecx
	movl	%cs:(r_edx), %edx
	movl	%cs:(r_ebp), %ebp
	movl	%cs:(r_esi), %esi
	movl	%cs:(r_edi), %edi
	movl	%cs:(r_esp), %esp

	/* Do a FAR CALL if there's no interrupt */
	cmpb	$0, %cs:(rm_int)
	je	do_call

	.byte	0xcd
rm_int:	.byte	0x03

	jmp	skip_call

do_call:

	.byte	0x9a
rm_ip:	.word	0
rm_cs:	.word	0

skip_call:

	/* Save the registers - we ignore %esp */
	movl	%eax, %cs:(r_eax)
	movl	%ebx, %cs:(r_ebx)
	movl	%ecx, %cs:(r_ecx)
	movl	%edx, %cs:(r_edx)
	movl	%ebp, %cs:(r_ebp)
	movl	%esi, %cs:(r_esi)
	movl	%edi, %cs:(r_edi)
	movw	%ds, %ax
	movw	%ax, %cs:(r_ds)
	movw	%es, %ax
	movw	%ax, %cs:(r_es)

	/* Store our flags */
	pushfl
	popl	%eax
	movl	%eax, %cs:(r_flags)

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
	.long	rc32_return
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

#ifdef X86_64
cpu_64bit_capable:
	.long	0
#endif

rm_regs:
r_eax:	.long	0
r_ebx:	.long	0
r_ecx:	.long	0
r_edx:	.long	0
r_ebp:	.long	0
r_esi:	.long	0
r_edi:	.long	0
r_esp:	.long	0
r_ds:	.word	0
r_es:	.word	0
r_ss:	.word	0
r_flags:.long	0
r_int:	.byte	0
r_cs:	.word	0
r_ip:	.word	0
	
realcall_esp:	.long	0

/*
 * We need a realmode buffer; this is used to read sectors etc.
 */
rm_buffer:
	.space	1024

/*
 * Stack used for realmode work. This must be kept here since
 * it must exist in the same segment as the realmode code.
 */
	.space	1024
rm_stack:

