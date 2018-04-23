#include "kernel/x86/apic.h"
#include "kernel/x86/ioapic.h"
#include "kernel-md/vm.h"

X86_IOAPIC::X86_IOAPIC()
	: IRQSource(0, 0)
{
	// Note that we expect Initialize() to be called. This is not very OOP-y, but
	// it ensures that our vtable is in place
}

void
X86_IOAPIC::Write(uint32_t reg, uint32_t val)
{
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOREGSEL) = reg & 0xff;
	*reinterpret_cast<volatile uint32_t*>(ioa_addr + IOWIN) = val;
}

uint32_t
X86_IOAPIC::Read(uint32_t reg)
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
X86_IOAPIC::Initialize(uint8_t id, addr_t addr, int base_irq)
{
	ioa_id = id;
	ioa_addr = addr;
	is_first = base_irq;
	// Fetch IOAPIC version register; this contains the number of interrupts supported
	is_count = (Read(IOAPICVER) >> 16) & 0xff;

	irq::RegisterSource(*this);
}

/* vim:set ts=2 sw=2: */
