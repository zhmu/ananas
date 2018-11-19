#include "kernel/x86/apic.h"
#include "kernel/x86/ioapic.h"
#include "kernel-md/vm.h"
#include "kernel/lib.h"

X86_IOAPIC::X86_IOAPIC(uint8_t id, addr_t addr, int base_irq)
	: ioa_addr(addr), ioa_irq_base(base_irq)
{
	// Program the IOAPIC ID; some BIOS'es do not do this
	WriteRegister(IOAPICID, id << 24);

	// Fetch IOAPIC version register; this contains the number of interrupts supported
	int irq_count = (ReadRegister(IOAPICVER) >> 16) & 0xff;
	ioa_pins.resize(irq_count);

	// Start by masking everything so we don't get any spurious interrupts we are
	// not ready for
	for(size_t n = 0; n < ioa_pins.size(); n++)
		Mask(n);
}

int X86_IOAPIC::GetFirstInterruptNumber() const
{
	return ioa_irq_base;
}

int X86_IOAPIC::GetInterruptCount() const
{
	return ioa_pins.size();
}

void
X86_IOAPIC::WriteRegister(uint32_t reg, uint32_t val)
{
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) = val;
}

uint32_t
X86_IOAPIC::ReadRegister(uint32_t reg)
{
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
	return *reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN);
}

void
X86_IOAPIC::Mask(int no)
{
	uint32_t reg = IOREDTBL + no * 2;
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) |= MASKED;
}

void
X86_IOAPIC::Unmask(int no)
{
	uint32_t reg = IOREDTBL + no * 2;
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) &= ~MASKED;
}

void
X86_IOAPIC::AcknowledgeAll()
{
	*(volatile uint32_t*)(PTOKV(LAPIC_BASE) + LAPIC_EOI) = 0;
}

void
X86_IOAPIC::Acknowledge(int no)
{
	AcknowledgeAll();
}

void
X86_IOAPIC::SetupPin(int pin, int dest_irq, Polarity polarity, TriggerLevel triggerLevel, int dest_apic_id)
{
	KASSERT(dest_irq >= 0x10 && dest_irq <= 0xff, "invalid interrupt destination irq %d", dest_irq);
	uint32_t reg = IOREDTBL + (pin * 2);
	uint64_t val = DESTMOD_PHYSICAL | DELMOD_FIXED | dest_irq;
	if (polarity == Polarity::Low)
		val |= INTPOL;
	if (triggerLevel == TriggerLevel::Level)
		val |= TRIGGER_LEVEL;
	WriteRegister(reg, val);
	WriteRegister(reg + 1, dest_apic_id << 24);
}

/* vim:set ts=2 sw=2: */
