/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __X86_APIC_H__
#define __X86_APIC_H__

#define LAPIC_BASE 0xfee00000
#define LAPIC_SIZE PAGE_SIZE

#define LAPIC_ID 0x0020  /* Local APIC ID Register */
#define LAPIC_VER 0x0030 /* Local APIC Version Register */
#define LAPIC_TPR 0x0080 /* Task Priority Register */
#define LAPIC_APR 0x0090 /* Arbitration Priority Register */
#define LAPIC_PPR 0x00a0 /* Processor Priority Register */
#define LAPIC_EOI 0x00b0 /* End-Of-Interrupt Register */
#define LAPIC_LD 0x00d0  /* Local Destination Register */
#define LAPIC_DF 0x00e0  /* Destination Format Register */
#define LAPIC_SVR 0x00f0 /* Spurious Vector Register */
#define LAPIC_SVR_APIC_EN 0x0100
#define LAPIC_ISR 0x0100                   /* In-Service Register */
#define LAPIC_TMR 0x0180                   /* Trigger Mode Register */
#define LAPIC_IRR 0x0200                   /* Interrupt Request Register */
#define LAPIC_ESR 0x0280                   /* Error Status Register */
#define LAPIC_ICR_LO 0x0300                /* Interrupt Control Register */
#define LAPIC_ICR_DELIVERY_FIXED (0 << 8)  /* Fixed vector */
#define LAPIC_ICR_DELIVERY_LOWEST (1 << 8) /* Fixed vector to lowest priority CPU */
#define LAPIC_ICR_DELIVERY_SMI (2 << 8)    /* SMI */
#define LAPIC_ICR_DELIVERY_NMI (4 << 8)    /* NMI */
#define LAPIC_ICR_DELIVERY_INIT (5 << 8)   /* INIT request */
#define LAPIC_ICR_DELIVERY_SIPI (6 << 8)   /* Startup request */
#define LAPIC_ICR_STATUS_IDLE (0 << 12)
#define LAPIC_ICR_STATUS_PENDING (1 << 12)
#define LAPIC_ICR_LEVEL_DEASSERT (0 << 14)
#define LAPIC_ICR_LEVEL_ASSERT (1 << 14)
#define LAPIC_ICR_TRIGGER_EDGE (0 << 15)
#define LAPIC_ICR_TRIGGER_LEVEL (1 << 15)
#define LAPIC_ICR_DEST_FIELD (0 << 18)        /* No destination shorthand */
#define LAPIC_ICR_DEST_SELF (1 << 18)         /* Talking to myself */
#define LAPIC_ICR_DEST_ALL_INC_SELF (2 << 18) /* All including self */
#define LAPIC_ICR_DEST_ALL_EXC_SELF (3 << 18) /* All excluding self */
#define LAPIC_ICR_HI 0x0310
#define LAPIC_LVT_TR 0x0320 /* LVT Timer Register */
#define LAPIC_LVT_TM_ONESHOT 0
#define LAPIC_LVT_TM_PERIODIC (1 << 17)
#define LAPIC_LVT_M (1 << 16)  /* Masked */
#define LAPIC_LVT_DS (1 << 12) /* Delivery Status */
#define LAPIC_LVT_TSR 0x0330   /* LVT Thermal Sensor Register */
#define LAPIC_LVT_PMCR 0x0340  /* LVT Performance Monitoring Counters Register */
#define LAPIC_LVT_LINT0 0x0350 /* LVT LINT0 Register */
#define LAPIC_LVT_LINT1 0x0360 /* LVT LINT0 Register */
#define LAPIC_LVT_ER 0x0370    /* LVT Error Register */
#define LAPIC_LVT_ICR 0x0380   /* LVT Initial Count Register (Timer) */
#define LAPIC_LVT_CCR 0x0390   /* LVT Current Count Register (Timer) */
#define LAPIC_LVT_DCR 0x03e0   /* LVT Divide Confiration Register (Timer) */
#define LAPIC_LVT_DV_2 0x0     /* Divide by 2 */
#define LAPIC_LVT_DV_4 0x1     /* Divide by 4 */
#define LAPIC_LVT_DV_8 0x2     /* Divide by 8 */
#define LAPIC_LVT_DV_16 0x3    /* Divide by 16 */
#define LAPIC_LVT_DV_32 0x8    /* Divide by 32 */
#define LAPIC_LVT_DV_64 0x9    /* Divide by 64 */
#define LAPIC_LVT_DV_128 0xa   /* Divide by 128 */
#define LAPIC_LVT_DV_1 0xb     /* Divide by 1 */

#endif /* __X86_APIC_H__ */
