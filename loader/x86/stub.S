.text
.code16
.globl	entry, realcall
.globl	rm_buffer
#ifdef X86_64
.globl	cpu_64bit_capable
#endif
#ifdef PXE
.globl	pxe_trampoline
#endif
#ifdef DEBUG_CALLS
.globl	debug_calls
#endif

#include <loader/x86-asm.h>
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
	movw	$REALMODE_STACK, %sp

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
	movl	$0x7c00, %esp			/* 0:0x7c00 downwards is our stack */

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

#ifdef DEBUG_CALLS
	movb	0x417, %al			/* get keyboard status flags... */
	testb	$2, %al				/* ...and see whether lshift is pressed... */
	jz	skip_debugcall			/* ... if not, don't activate call debugging */

	incl	(debug_calls)

skip_debugcall:
#endif
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
	 *
	 * Prototype is: void realcall(struct REALMODE_REGS*);
	 */
	cli

	/*
	 * We must store %esp (which we have to do in a temporary register as it will
	 * get nuked. Note that we also have to save %ebp as the compiler expects this.
	 */
	pushl	%ebp
	movl	%esp, realcall_esp
	movl	8(%esp), %ebx
	movl	%ebx, realcall_regs

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

	/*
	 * First of all, start with a clean data segment; this will
	 * actually make our RM_xxx(%bx) macro's work.
	 */
	xorw	%ax, %ax
	movw	%ax, %ds

	/* Patch the interrupt number and call pointer. Hey, this is realmode - anything goes! */
	movb	RM_INT(%bx), %al
	movb	%al, (rm_int)
	movw	RM_CS(%bx), %ax
	movw	%ax, (rm_cs)
	movw	RM_IP(%bx), %ax
	movw	%ax, (rm_ip)

	/* Update %es/%ss segments; we'll do %ds later */
	movw	RM_ES(%bx), %ax
	movw	%ax, %es
	movw	RM_SS(%bx), %ax
	movw	%ax, %ss

	/* Restore most of our registers - not yet %eax */
	movl	RM_EAX(%bx), %eax
	movl	RM_ECX(%bx), %ecx
	movl	RM_EDX(%bx), %edx
	movl	RM_EBP(%bx), %ebp
	movl	RM_ESI(%bx), %esi
	movl	RM_EDI(%bx), %edi
	movl	RM_ESP(%bx), %esp
	pushl	RM_EFLAGS(%ebx)
	popfl

	/*
	 * We need to store %eax someplace safe, as we'll still need to
	 * fix %ebx, %ds.
	 */
	movl	%eax, realcall_eax
	movw	RM_DS(%bx), %ax
	movw	%ax, %ds
	movl	%cs:RM_EBX(%bx), %ebx
	movl	%cs:realcall_eax, %eax

	/* Enable interrupts */
	sti

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
	movl	%eax, %cs:realcall_eax
	movl	%ebx, %cs:realcall_ebx
	movl	%cs:realcall_regs, %ebx

	/* Store %ds first; we can restore it again to get rid of the %cs prefixes */
	movw	%ds, %ax
	movw	%ax, %cs:RM_DS(%bx)
	xorw	%ax, %ax
	movw	%ax, %ds

	/* Save all other the registers */
	movl	realcall_eax, %eax
	movl	%eax, RM_EAX(%bx)
	movl	realcall_ebx, %eax
	movl	%eax, RM_EBX(%bx)
	movl	%ecx, RM_ECX(%bx)
	movl	%edx, RM_EDX(%bx)
	movl	%ebp, RM_EBP(%bx)
	movl	%esi, RM_ESI(%bx)
	movl	%edi, RM_EDI(%bx)
	movl	%esp, RM_ESP(%bx)
	movw	%es, %ax
	movw	%ax, RM_ES(%bx)

	/* Store our flags */
	pushfl
	popl	%eax
	movl	%eax, RM_EFLAGS(%bx)

	/* Nuke whatever flags the BIOS gave us */
	pushl	$0
	popfl

	/* Ensure interrupts are not set - this would hurt us */
	cli

	/* Ensure we use the correct GDT */
	lgdt	%cs:(gdtr)

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

#ifdef PXE
pxe_trampoline:
	/* PXE wants stuff to be passed using the stack */
	pushl	%ebx		/* Buffer address */
	pushw	%ax		/* Function number */

	/* Fake an lcall */
	pushw	%cs		/* Return segment */
	pushw	$pxe_return	/* Return offset */
	pushl	%ecx
	lret

pxe_return:
	/* Throw away the stuff we put on the stack */
	addw	$6, %sp
	lret
#endif

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

/* Temporary registers used during mode switches */
realcall_regs:	.long	0
realcall_esp:	.long	0
realcall_eax:	.long	0
realcall_ebx:	.long	0

#ifdef DEBUG_CALLS
debug_calls:	.long	0
#endif
