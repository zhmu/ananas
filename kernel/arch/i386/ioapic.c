#include <machine/apic.h>
#include <machine/ioapic.h>
#include <ananas/lib.h>

void
ioapic_write(struct IA32_IOAPIC* apic, uint32_t reg, uint32_t val)
{
	*(volatile uint32_t*)(apic->ioa_addr + IOREGSEL) = reg & 0xff;
	*(volatile uint32_t*)(apic->ioa_addr + IOWIN) = val;
}

uint32_t
ioapic_read(struct IA32_IOAPIC* apic, uint32_t reg)
{
	*(volatile uint32_t*)(apic->ioa_addr + IOREGSEL) = reg & 0xff;
	return *(volatile uint32_t*)(apic->ioa_addr + IOWIN);
}

static errorcode_t
ioapic_mask(struct IRQ_SOURCE* source, int no)
{
	panic("not implemented");
}

static errorcode_t
ioapic_unmask(struct IRQ_SOURCE* source, int no)
{
	panic("not implemented");
}

void
ioapic_ack(struct IRQ_SOURCE* source, int no)
{
	struct IA32_IOAPIC* ioapic = source->is_privdata;
	(void)ioapic;
	*((uint32_t*)LAPIC_EOI) = 0;
}

void
ioapic_register(struct IA32_IOAPIC* ioapic, int base)
{
	/* Fetch IOAPIC version register; this contains the number of interrupts supported */
	uint32_t num_ints = ((ioapic_read(ioapic, IOAPICVER) >> 16) & 0xff) + 1;

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
