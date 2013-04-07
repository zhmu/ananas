#ifndef __I386_APIC_H__
#define __I386_APIC_H__

#define LAPIC_BASE	0xfee00000
#define LAPIC_SIZE	PAGE_SIZE

#define LAPIC_ID	0xfee00020		/* Local APIC ID Register */
#define LAPIC_VER	0xfee00030		/* Local APIC Version Register */
#define LAPIC_TPR	0xfee00080		/* Task Priority Register */
#define LAPIC_APR	0xfee00090		/* Arbitration Priority Register */
#define LAPIC_PPR	0xfee000a0		/* Processor Priority Register */
#define LAPIC_EOI	0xfee000b0		/* End-Of-Interrupt Register */
#define LAPIC_LD	0xfee000d0		/* Local Destination Register */
#define LAPIC_DF	0xfee000e0		/* Destination Format Register */
#define LAPIC_SVR	0xfee000f0		/* Spurious Vector Register */
#define  LAPIC_SVR_APIC_EN	0x0100
#define LAPIC_ISR	0xfee00100		/* In-Service Register */
#define LAPIC_TMR	0xfee00180		/* Trigger Mode Register */
#define LAPIC_IRR	0xfee00200		/* Interrupt Request Register */
#define LAPIC_ESR	0xfee00280		/* Error Status Register */
#define LAPIC_ICR_LO	0xfee00300		/* Interrupt Control Register */
#define  LAPIC_ICR_DELIVERY_FIXED	(0 << 8)	/* Fixed vector */
#define  LAPIC_ICR_DELIVERY_LOWEST	(1 << 8)	/* Fixed vector to lowest priority CPU */
#define  LAPIC_ICR_DELIVERY_SMI		(2 << 8)	/* SMI */
#define  LAPIC_ICR_DELIVERY_NMI		(4 << 8)	/* NMI */
#define  LAPIC_ICR_DELIVERY_INIT	(5 << 8)	/* INIT request */
#define  LAPIC_ICR_DELIVERY_SIPI	(6 << 8)	/* Startup request */
#define  LAPIC_ICR_STATUS_IDLE		(0 << 12)
#define  LAPIC_ICR_STATUS_PENDING	(1 << 12)
#define  LAPIC_ICR_LEVEL_DEASSERT	(0 << 14)
#define  LAPIC_ICR_LEVEL_ASSERT		(1 << 14)
#define  LAPIC_ICR_TRIGGER_EDGE		(0 << 15)
#define  LAPIC_ICR_TRIGGER_LEVEL	(1 << 15)
#define  LAPIC_ICR_DEST_FIELD		(0 << 18)	/* No destination shorthand */
#define  LAPIC_ICR_DEST_SELF		(1 << 18)	/* Talking to myself */
#define  LAPIC_ICR_DEST_ALL_INC_SELF	(2 << 18)	/* All including self */
#define  LAPIC_ICR_DEST_ALL_EXC_SELF	(3 << 18)	/* All excluding self */
#define LAPIC_ICR_HI	0xfee00310
#define LAPIC_LVT_TR	0xfee00320		/* LVT Timer Register */
#define LAPIC_LVT_TSR	0xfee00330		/* LVT Thermal Sensor Register */
#define LAPIC_LVT_PMCR	0xfee00340		/* LVT Performance Monitoring Counters Register */
#define LAPIC_LVT_LINT0	0xfee00350		/* LVT LINT0 Register */
#define LAPIC_LVT_LINT1	0xfee00360		/* LVT LINT0 Register */
#define LAPIC_LVT_ER	0xfee00370		/* LVT Error Register */
#define LAPIC_LVT_ICR	0xfee00380		/* LVT Initial Count Register (Timer) */
#define LAPIC_LVT_CCR	0xfee00390		/* LVT Current Count Register (Timer) */
#define LAPIC_LVT_DCR	0xfee003e0		/* LVT Divide Confiration Register (Timer) */

#endif /* __I386_APIC_H__ */
