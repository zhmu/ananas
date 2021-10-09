/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/util/algorithm.h>
#include "kernel-md/ioapic.h"
#include "kernel-md/lapic.h"
#include "kernel-md/vm.h"
#include "kernel/lib.h"

namespace
{
    constexpr int numberOfISAVectors = 16;

} // unnamed namespace

X86_IOAPIC::X86_IOAPIC(uint8_t id, addr_t addr, int base_irq)
    : ioa_addr(addr), ioa_id(id), ioa_irq_base(base_irq)
{
    // Program the IOAPIC ID; some BIOS'es do not do this
    WriteRegister(IOAPICID, id << 24);

    // Fetch IOAPIC version register; this contains the number of interrupts supported
    int irq_count = (ReadRegister(IOAPICVER) >> 16) & 0xff;
    ioa_pins.resize(irq_count);

    // Initially, come up with some sane defaults which mask everything
    for (size_t pinNumber = 0; pinNumber < ioa_pins.size(); pinNumber++) {
        auto& pin = ioa_pins[pinNumber];
        pin.p_pin = pinNumber;
        pin.p_vector = ioa_irq_base + pinNumber;
        pin.p_deliverymode = DeliveryMode::Fixed;
        pin.p_masked = true;
    }
}

int X86_IOAPIC::GetFirstInterruptNumber() const { return ioa_irq_base; }

int X86_IOAPIC::GetInterruptCount() const { return ioa_pins.size(); }

void X86_IOAPIC::WriteRegister(uint32_t reg, uint32_t val)
{
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) = val;
}

uint32_t X86_IOAPIC::ReadRegister(uint32_t reg)
{
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
    return *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN);
}

void X86_IOAPIC::Mask(int no)
{
    KASSERT(no >= 0 && no < static_cast<int>(ioa_pins.size()), "masking out-of-bounds irq %d", no);
    auto& pin = FindPinByVector(0x20 + no);
    if (pin.p_triggerlevel == TriggerLevel::Edge)
        return; // do not mask edge-triggered interrupts

    pin.p_masked = true;

    uint32_t reg = IOREDTBL + pin.p_pin * 2;
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) |= MASKED;
}

void X86_IOAPIC::Unmask(int no)
{
    KASSERT(
        no >= 0 && no < static_cast<int>(ioa_pins.size()), "unmasking out-of-bounds irq %d", no);
    auto& pin = FindPinByVector(0x20 + no);
    if (!pin.p_masked) return;
    pin.p_masked = false;

    uint32_t reg = IOREDTBL + pin.p_pin * 2;
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
    *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) &= ~MASKED;
}

void X86_IOAPIC::AcknowledgeAll() { *(volatile uint32_t*)(PTOKV(LAPIC_BASE) + LAPIC_EOI) = 0; }

void X86_IOAPIC::Acknowledge(int no) { AcknowledgeAll(); }

void X86_IOAPIC::ConfigurePin(
    int pinNumber, int dest_vector, int dest_cpu, DeliveryMode deliverymode, Polarity polarity,
    TriggerLevel triggerLevel)
{
    KASSERT(pinNumber >= 0 && pinNumber < ioa_pins.size(), "invalid pin %d", pinNumber);

    auto& pin = ioa_pins[pinNumber];
    pin.p_vector = dest_vector;
    pin.p_cpu = dest_cpu;
    pin.p_deliverymode = deliverymode;
    pin.p_polarity = polarity;
    pin.p_triggerlevel = triggerLevel;
}

void X86_IOAPIC::ApplyPin(const X86_IOAPIC::Pin& pin)
{
    uint64_t low = DESTMOD_PHYSICAL;
    switch (pin.p_deliverymode) {
        default:
        case DeliveryMode::Fixed:
            low |= DELMOD_FIXED | pin.p_vector;
            break;
        case DeliveryMode::LowestPriority:
            low |= DELMOD_LOWPRIO;
            break;
        case DeliveryMode::SMI:
            low |= DELMOD_SMI;
            break;
        case DeliveryMode::NMI:
            low |= DELMOD_NMI;
            break;
        case DeliveryMode::INIT:
            low |= DELMOD_INIT;
            break;
        case DeliveryMode::ExtInt:
            low |= DELMOD_EXTINT;
            break;
    }
    low |= (pin.p_polarity == Polarity::Low) ? INTPOL : 0;
    low |= (pin.p_triggerlevel == TriggerLevel::Level) ? TRIGGER_LEVEL : TRIGGER_EDGE;
    if (pin.p_masked)
        low |= MASKED;
    uint64_t high = pin.p_cpu << 24;

    uint32_t reg = IOREDTBL + (pin.p_pin * 2);
    WriteRegister(reg, low);
    WriteRegister(reg + 1, high);
}

void X86_IOAPIC::ApplyConfiguration()
{
    util::for_each(ioa_pins, [this](const Pin& pin) { ApplyPin(pin); });
}

void X86_IOAPIC::DumpConfiguration()
{
    constexpr auto resolveDeliveryMode = [](DeliveryMode dm) {
        switch (dm) {
            case DeliveryMode::Fixed:
                return "FIXED";
            case DeliveryMode::LowestPriority:
                return "LPRIO";
            case DeliveryMode::SMI:
                return "SMI";
            case DeliveryMode::NMI:
                return "NMI";
            case DeliveryMode::INIT:
                return "INIT";
            case DeliveryMode::ExtInt:
                return "EXTINT";
            default:
                return "???";
        }
    };
    constexpr auto resolvePolarity = [](Polarity polarity) {
        switch (polarity) {
            case Polarity::Low:
                return "lo";
            case Polarity::High:
                return "hi";
            default:
                return "???";
        }
    };
    constexpr auto resolveTriggerLevel = [](TriggerLevel triggerlevel) {
        switch (triggerlevel) {
            case TriggerLevel::Level:
                return "level";
            case TriggerLevel::Edge:
                return "edge";
            default:
                return "???";
        }
    };

    kprintf("ioapic id %d: addr %p base %d\n", ioa_id, ioa_addr, ioa_irq_base);
    kprintf("pin vector cpu masked   mode pol trigger\n");
    for (const auto& pin : ioa_pins) {
        kprintf(
            "%3d    %3d  %2d     %c  %6s %3s %s\n", pin.p_pin, pin.p_vector, pin.p_cpu,
            pin.p_masked ? 'Y' : '-', resolveDeliveryMode(pin.p_deliverymode),
            resolvePolarity(pin.p_polarity), resolveTriggerLevel(pin.p_triggerlevel));
    }
}

X86_IOAPIC::Pin& X86_IOAPIC::FindPinByVector(int vector)
{
    auto it = util::find_if(ioa_pins, [vector](const Pin& pin) { return pin.p_vector == vector; });
    if (it == ioa_pins.end()) {
        kprintf("vector %d not found!!\n", vector);
        DumpConfiguration();
    }
    KASSERT(it != ioa_pins.end(), "vector %d not found", vector);
    return *it;
}
