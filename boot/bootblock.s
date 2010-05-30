.code16
.text
.globl	entry

entry:
	/*
	 * The BIOS loads a whole sector, which isn't quite enough these days.
	 * Thus, this code will just the first few sectors (this is always
	 * available on a fixed disk) and jump to that.
	 *
	 * We are loaded at 0x7c00; it makes most sense to just load the data
	 * at 0x7e00 as there's plenty of memory there - and more importantly,
	 * we know our kernel will be loaded at the 1MB border, so this will
	 * work out nicely.
	 *
	 * However, PXE will also load the code to that location, so we'd be
	 * better off just to place our loader there. This means we'll
	 * just move our code to, say, 0x500... *sigh*
	 */
	nop

	/* First of all, reset all high words */
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	movzxw	%dx, %edx
	xorl	%esi, %esi
	xorl	%edi, %edi
	xorl	%ebp, %ebp

	/*
	 * Place our stack somewhere safe; 0x7c00 downwards makes
	 * most sense.
	 */
	movw	%cs, %ax
	movw	%ax, %ds
	xorw	%ax, %ax
	movw	%ax, %ss
	movw	$0x7c00, %sp

	/* Now, move our code... */
	movw	$0x50, %ax
	movw	%ax, %es
	movw	$entry, %si
	xorw	%di, %di
	movw	$0x200, %cx
	rep	movsw
	/* ... and visit the new location */
	.byte	0xea
	.word	(n - entry)
	.word	0x50

n:	nop
	movw	$0x7c0, %ax
	movw	%ax, %es
	xorw	%bx, %bx		/* es:bx = entry */

	/* First of all, reset the disk system */
	xorw	%ax, %ax
	int	$0x13

	movw	$0x023f, %ax		/* bios: load a sector */
	movw	$0x0002, %cx		/* cyl 0, sector 2 */
	movb	$0x00, %dh		/* head 0 */
	int	$0x13
	jnc	read_ok

	/* uh-uh... */
	movw	$read_err, %si
	movb	$0x0e, %ah		/* video: tty output */
putch:	lodsb
	orb	%al, %al
	jz	putch_done
	int	$0x10
	jmp	putch

putch_done:
	hlt

read_ok:
	ljmp	$0, $0x7c00

read_err:
	.ascii	"read error"
	.byte	0
