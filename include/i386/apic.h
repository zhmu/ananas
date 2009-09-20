#ifndef __I386_APIC_H__
#define __I386_APIC_H__

#define LAPIC_BASE	0xfee00000
#define LAPIC_SIZE	PAGE_SIZE

#define LAPIC_ICR_LOW	0xfee00300		/* Interrupt Control Register */
#define LAPIC_ICR_HI	0xfee00310
#define LAPIC_SVR	0xfee000f0		/* Spurious Vector Register */
#define  LAPIC_SVR_APIC_EN	0x0100

#endif /* __I386_APIC_H__ */
