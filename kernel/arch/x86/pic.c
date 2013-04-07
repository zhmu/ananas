#include <ananas/x86/io.h>
#include <ananas/x86/pic.h>
#include <ananas/irq.h>
#include <ananas/lib.h>

static errorcode_t pic_mask(struct IRQ_SOURCE* is, int no);
static errorcode_t pic_unmask(struct IRQ_SOURCE* is, int no);
static void pic_ack(struct IRQ_SOURCE* is, int no);

static struct IRQ_SOURCE pic_source = {
	.is_first = 0,
	.is_count = 16,
	.is_mask = pic_mask,
	.is_unmask = pic_unmask,
	.is_ack = pic_ack
};

void
x86_pic_init()
{
	/*
	 * Remap the interrupts; by default, IRQ 0-7 are mapped to interrupts 0x08 -
	 * 0x0f and IRQ 8-15 to 0x70 - 0x77. We remap IRQ 0-15 to 0x20-0x2f (since
	 * 0..0x1f is reserved by Intel).
	 */
#define IO_WAIT() do { int i = 10; while (i--) /* NOTHING */ ; } while (0);

	/* Start initialization: the PIC will wait for 3 command bta ytes */
	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); IO_WAIT();
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); IO_WAIT();
	/* Data byte 1 is the interrupt vector offset - program for interrupt 0x20-0x2f */
	outb(PIC1_DATA, 0x20); IO_WAIT();
	outb(PIC2_DATA, 0x28); IO_WAIT();
	/* Data byte 2 tells the PIC how they are wired */
	outb(PIC1_DATA, 0x04); IO_WAIT();
	outb(PIC2_DATA, 0x02); IO_WAIT();
	/* Data byte 3 contains environment flags */
	outb(PIC1_DATA, ICW4_8086); IO_WAIT();
	outb(PIC2_DATA, ICW4_8086); IO_WAIT();
	/* Enable all interrupts */
	outb(PIC1_DATA, 0);
	outb(PIC2_DATA, 0);

#undef IO_WAIT

	/* Register the PIC as interrupt source */
	irqsource_register(&pic_source);
}

void
x86_pic_mask_all()
{
	/*
	 * Mask all PIC interrupts; mainly used so that we get the PIC to a known
	 * state before we decide whether to use the PIC or APIC.
	 */
	outb(PIC1_DATA, 0xff);
	outb(PIC2_DATA, 0xff);
}

static errorcode_t
pic_mask(struct IRQ_SOURCE* is, int no)
{
	panic("TODO");
}

static errorcode_t
pic_unmask(struct IRQ_SOURCE* is, int no)
{
	panic("TODO");
}

static void
pic_ack(struct IRQ_SOURCE* is, int no)
{
	outb(0x20, 0x20);
	if (no >= 8)
		outb(0xa0, 0x20);
}

int
md_interrupts_enabled()
{
	int i;

#ifdef __i386__
	__asm(
		"pushfl\n"
		"pop 	%%eax\n"
	: "=a" (i));
#elif defined(__amd64__)
	__asm(
		"pushfq\n"
		"pop 	%%rax\n"
	: "=a" (i));
#endif /* __i386__ */
	return (i & 0x200);
}

/* vim:set ts=2 sw=2: */
