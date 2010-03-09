/*
 * Low-level initialization code.
 *
 */
.text

#include <machine/param.h>

.global __entry, idt, gdt, temp_pt_entry

/* Macro used to reallocate a virtual address to physical */
#define R(x) ((x) - KERNBASE)

__entry:
	/* Set all extra descriptors to whatever DS is pointing to */
	mov	%ds, %ax
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

	/* don't trust what the bootloader gives us, set up a stack */
	mov	$R(KERNEL_STACK), %esp

	/* disable serial interrupts */
	mov	$0x3f8+1,%dx
	mov	$0x00, %al
	outb	%al, %dx

	/* enable DLAB (sets baud rate divisor) */
	mov	$0x3f8+3,%dx
	mov	$0x80, %al
	outb	%al, %dx

	/* set divisor low byte for 9600 baud */
	mov	$0x3f8+0,%dx
	mov	$0x0c, %al
	outb	%al, %dx

	/* set divisor hi byte for 9600 baud */
	mov	$0x3f8+1,%dx
	mov	$0x0, %al
	outb	%al, %dx

	/* 8 bits, no parity and 1 stop bit */
	mov	$0x3f8+3,%dx
	mov	$0x3, %al
	outb	%al, %dx

	/* enable FIFO, clear it and set it for a 14 byte threshold */
	mov	$0x3f8+2,%dx
	mov	$0xc7, %al
	outb	%al, %dx

	/* re-enable interrupts */
	mov	$0x3f8+4,%dx
	mov	$0x0b, %al
	outb	%al, %dx

	jmp	md_startup

.align 32

	.space	0x2000
KERNEL_STACK:

/*
 * Reserve space for the Global Descriptor Table / Interrupt Descriptor tables.
 * This is done here because their size is always fixed.
 */
gdt:
	.space	GDT_NUM_ENTRIES * 8

idt:
	.space	IDT_NUM_ENTRIES * 8

/*
 * Reserve space for a temporary page directory entry; this is needed because
 * it is the only way we can pre-setup a mapping.
 */
.align 4096
temp_pt_entry:
	.space	PAGE_SIZE
