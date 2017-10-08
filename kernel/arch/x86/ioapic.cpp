#include <ananas/error.h>
#include "kernel/x86/apic.h"
#include "kernel/x86/ioapic.h"
#include "kernel-md/vm.h"

void
ioapic_write(struct X86_IOAPIC* ioapic, uint32_t reg, uint32_t val)
{
	*(volatile uint32_t*)(ioapic->ioa_addr + IOREGSEL) = reg & 0xff;
	*(volatile uint32_t*)(ioapic->ioa_addr + IOWIN) = val;
}

uint32_t
ioapic_read(struct X86_IOAPIC* ioapic, uint32_t reg)
{
	*(volatile uint32_t*)(ioapic->ioa_addr + IOREGSEL) = reg & 0xff;
	return *(volatile uint32_t*)(ioapic->ioa_addr + IOWIN);
}

static errorcode_t
ioapic_mask(struct IRQ_SOURCE* source, int no)
{
	struct X86_IOAPIC* ioapic = static_cast<X86_IOAPIC*>(source->is_privdata);
	uint32_t reg = IOREDTBL + no * 2;
	*(volatile uint32_t*)(ioapic->ioa_addr + IOREGSEL) = reg & 0xff;
	*(volatile uint32_t*)(ioapic->ioa_addr + IOWIN) |= MASKED;
	return ananas_success();
}

static errorcode_t
ioapic_unmask(struct IRQ_SOURCE* source, int no)
{
	struct X86_IOAPIC* ioapic = static_cast<X86_IOAPIC*>(source->is_privdata);
	uint32_t reg = IOREDTBL + no * 2;
	*(volatile uint32_t*)(ioapic->ioa_addr + IOREGSEL) = reg & 0xff;
	*(volatile uint32_t*)(ioapic->ioa_addr + IOWIN) &= ~MASKED;
	return ananas_success();
}

void
ioapic_ack(struct IRQ_SOURCE* source, int no)
{
	*(volatile uint32_t*)(PTOKV(LAPIC_BASE) + LAPIC_EOI) = 0;
}

void
ioapic_register(struct X86_IOAPIC* ioapic, int base)
{
	/* Fetch IOAPIC version register; this contains the number of interrupts supported */
	uint32_t num_ints = ((ioapic_read(ioapic, IOAPICVER) >> 16) & 0xff);

	/* Fill out the remaining structure members */
	ioapic->ioa_source.is_first = base;
	ioapic->ioa_source.is_count = num_ints;
	ioapic->ioa_source.is_privdata = ioapic;
	ioapic->ioa_source.is_mask = ioapic_mask;
	ioapic->ioa_source.is_unmask = ioapic_unmask;
	ioapic->ioa_source.is_ack = ioapic_ack;
	irqsource_register(&ioapic->ioa_source);
}

/* vim:set ts=2 sw=2: */
