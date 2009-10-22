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
