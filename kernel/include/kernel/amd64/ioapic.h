/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#pragma once

#include "kernel/irq.h"
#include <ananas/util/vector.h>

#define IOREGSEL 0x00000000 /* I/O Register Select */
#define IOWIN 0x00000010    /* I/O Window */

#define IOAPICID 0x00  /* IOAPIC ID */
#define IOAPICVER 0x01 /* IOAPIC Version */
#define IOAPICARB 0x02 /* IOAPIC Arbitration ID */
#define IOREDTBL 0x10  /* Redirection table */

#define DEST(n) ((n) << 56) /* Destination interrupt / CPU set */
#define MASKED (1 << 16)    /* Masked Interrupt */

#define TRIGGER_EDGE (0)        /* Trigger: Edge sensitive */
#define TRIGGER_LEVEL (1 << 15) /* Trigger: Level sensitive */
#define RIRR (1 << 14)          /* Remote IRR */
#define INTPOL (1 << 13)        /* Interrupt polarity */

#define DELIVS (1 << 12) /* Delivery status */

#define DESTMOD_PHYSICAL (0)
#define DESTMOD_LOGICAL (1 << 11)

#define DELMOD_FIXED (0)        /* Deliver on INTR signal */
#define DELMOD_LOWPRIO (1 << 8) /* Deliver on INTR, lowest prio */
#define DELMOD_SMI (2 << 8)     /* SMI interrupt, must be edge */
#define DELMOD_NMI (4 << 8)     /* NMI interrupt, must be edge */
#define DELMOD_INIT (5 << 8)    /* INIT IPI */
#define DELMOD_EXTINT (7 << 8)  /* External int, must be edge */

struct X86_IOAPIC final : irq::IRQSource {
    X86_IOAPIC(uint8_t id, addr_t addr, int base_irq);

    int GetFirstInterruptNumber() const override;
    int GetInterruptCount() const override;
    void Mask(int no) override;
    void Unmask(int no) override;
    void Acknowledge(int no) override;
    static void AcknowledgeAll();

    enum class DeliveryMode { Fixed, LowestPriority, SMI, NMI, INIT, ExtInt };

    enum class Polarity { Low, High };

    enum class TriggerLevel { Level, Edge };

    void ConfigurePin(
        int pinNumber, int dest_vector, int dest_cpu, DeliveryMode deliverymode, Polarity polarity,
        TriggerLevel triggerLevel);
    void ApplyConfiguration();
    void DumpConfiguration();

  private:
    void WriteRegister(uint32_t reg, uint32_t val);
    uint32_t ReadRegister(uint32_t reg);

    addr_t ioa_addr;
    int ioa_id;
    int ioa_irq_base;

    struct Pin {
        int p_pin;
        int p_vector;
        int p_cpu; // Note: APIC ID
        bool p_masked = true;
        DeliveryMode p_deliverymode;
        Polarity p_polarity;
        TriggerLevel p_triggerlevel;
    };
    Pin& FindPinByVector(int vector);
    void ApplyPin(const Pin& pin);

    util::vector<Pin> ioa_pins;
};
