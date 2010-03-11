#include <machine/io.h>
#include <sys/x86/pic.h>

void
x86_pic_remap() {
#define IO_WAIT() do { int i = 10; while (i--) /* NOTHING */ ; } while (0);

	unsigned int mask1 = inb(PIC1_DATA);
	unsigned int mask2 = inb(PIC2_DATA);
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
	/* Restore PIC masks */
	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);

#undef IO_WAIT
}

/* vim:set ts=2 sw=2: */
