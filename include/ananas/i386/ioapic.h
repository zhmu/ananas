#ifndef __I386_IOAPIC_H__
#define __I386_IOAPIC_H__

#define IOREGSEL		0x00000000	/* I/O Register Select */
#define IOWIN			0x00000010	/* I/O Window */

#define IOAPICID		0x00		/* IOAPIC ID */
#define IOAPICVER		0x01		/* IOAPIC Version */
#define IOAPICARB		0x02		/* IOAPIC Arbitration ID */
#define IOREDTBL		0x10		/* Redirection table */

#define DEST(n)			((n) << 56)	/* Destination interrupt / CPU set */
#define MASKED			(1 << 16)	/* Masked Interrupt */

#define TRIGGER_EDGE		(0)		/* Trigger: Edge sensitive */
#define TRIGGER_LEVEL		(1 << 15)	/* Trigger: Level sensitive */
#define RIRR			(1 << 14)	/* Remote IRR */
#define INTPOL			(1 << 13)	/* Interrupt polarity */

#define DELIVS			(1 << 12)	/* Delivery status */

#define DESTMOD_PHYSICAL	(0)
#define DESTMOD_LOGICAL		(1 << 11)

#define DELMOD_FIXED		(0)		/* Deliver on INTR signal */
#define DELMOD_LOWPRIO		(1 << 8)	/* Deliver on INTR, lowest prio */
#define DELMOD_SMI		(2 << 8)	/* SMI interrupt, must be edge */
#define DELMOD_NMI		(4 << 8)	/* NMI interrupt, must be edge */
#define DELMOD_INIT		(5 << 8)	/* INIT IPI */
#define DELMOD_EXTINT		(7 << 8)	/* External int, must be edge */


struct IA32_IOAPIC {
	uint8_t		ioapic_id;
	addr_t		addr;
};

#endif /* __I386_IOAPIC_H__ */
