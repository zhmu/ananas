#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/kmem.h>
#include <ananas/trace.h>
#include <ananas/vm.h>
#include <ananas/x86/apic.h>
#include <ananas/x86/ioapic.h>
#include <ananas/x86/smp.h>
#include <machine/param.h>
#include <machine/vm.h>
#include "../../dev/acpi/acpi.h"
#include "../../dev/acpi/acpica/acpi.h"

TRACE_SETUP;

extern struct X86_SMP_CONFIG smp_config;

/*
 * Maps one page of memory from physical address phys and returns the
 * destination address va, guaranteeing that 'va = PTOKV(phys)'.
 *
 * Note that on i386, this implies that phys must be >=KERNBASE.
 */
static void*
map_device(addr_t phys)
{
	void* va;
	unsigned int vm_flags = VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE;
#ifdef __amd64__
	va = kmem_map(phys, 1, vm_flags);
#else
	va = (void*)phys;
	md_kmap(phys, (addr_t)va, 1, vm_flags);
#endif
	return va;
}

errorcode_t
acpi_smp_init(int* bsp_apic_id)
{
	ACPI_TABLE_MADT* madt;
	if (ACPI_FAILURE(AcpiGetTable(ACPI_SIG_MADT, 0, (ACPI_TABLE_HEADER**)&madt)))
		return ANANAS_ERROR(NO_DEVICE);

	/*
	 * The MADT doesn't tell us which CPU is the BSP, but we do know it is our
	 * current CPU, so just asking would be enough.
	 */
	KASSERT(madt->Address == LAPIC_BASE, "lapic base unsupported");
	char* lapic_base = map_device(madt->Address);
	KASSERT((addr_t)lapic_base == PTOKV(madt->Address), "mis-mapped lapic (%p != %p)", lapic_base, PTOKV(madt->Address));
	/* Fetch our local APIC ID, we need to program it shortly */
	*bsp_apic_id = (*(volatile uint32_t*)(lapic_base + LAPIC_ID)) >> 24;
	/* Reset destination format to flat mode */
	*(volatile uint32_t*)(lapic_base + LAPIC_DF) = 0xffffffff;
	/* Ensure we are the logical destination of our local APIC */
	volatile uint32_t* v = (volatile uint32_t*)(lapic_base + LAPIC_LD);
	*v = (*v & 0x00ffffff) | 1 << (*bsp_apic_id + 24);
	/* Clear Task Priority register; this enables all LAPIC interrupts */
	*(volatile uint32_t*)(lapic_base + LAPIC_TPR) &= ~0xff;
	/* Finally, enable the APIC */
	*(volatile uint32_t*)(lapic_base + LAPIC_SVR) |= LAPIC_SVR_APIC_EN;
	
	/* First of all, walk through the MADT and just count everything */
	for (ACPI_SUBTABLE_HEADER* sub = (void*)(madt + 1);
	     sub < (ACPI_SUBTABLE_HEADER*)((char*)madt + madt->Header.Length);
	    sub = (ACPI_SUBTABLE_HEADER*)((char*)sub + sub->Length)) {
		switch(sub->Type) {
			case ACPI_MADT_TYPE_LOCAL_APIC: {
				ACPI_MADT_LOCAL_APIC* lapic = (ACPI_MADT_LOCAL_APIC*)sub;
				if (lapic->LapicFlags & ACPI_MADT_ENABLED)
					smp_config.cfg_num_cpus++;
				break;
			}
			case ACPI_MADT_TYPE_IO_APIC:
				smp_config.cfg_num_ioapics++;
				break;
		}
	}

	/*
	 * ACPI interrupt overrides will only list exceptions; this means we'll
	 * have to pre-allocate all ISA interrupts and let the overrides
	 * alter them.
	 */
	smp_config.cfg_num_ints = 16;

	/* XXX Kludge together an ISA bus for the IRQ routing code */
	smp_config.cfg_num_busses = 1;

	/* Allocate tables for the resources we found */
	smp_prepare_config(&smp_config);

	/* Create the ISA bus */
	smp_config.cfg_bus[0].id = 0;
	smp_config.cfg_bus[0].type = BUS_TYPE_ISA;

	/* Identity-map all interrupts */
	for (int n = 0; n < smp_config.cfg_num_ints; n++) {
		struct X86_INTERRUPT* interrupt = &smp_config.cfg_int[n];
		interrupt->source_no = n;
		interrupt->dest_no = n;
		interrupt->bus = &smp_config.cfg_bus[0];
		interrupt->ioapic = &smp_config.cfg_ioapic[0]; // XXX
	}

	/* Since we now got all memory set up, */
	int cur_cpu = 0, cur_ioapic = 0;
	for (ACPI_SUBTABLE_HEADER* sub = (void*)(madt + 1);
	     sub < (ACPI_SUBTABLE_HEADER*)((char*)madt + madt->Header.Length);
	    sub = (ACPI_SUBTABLE_HEADER*)((char*)sub + sub->Length)) {
		switch(sub->Type) {
			case ACPI_MADT_TYPE_LOCAL_APIC: {
				ACPI_MADT_LOCAL_APIC* lapic = (ACPI_MADT_LOCAL_APIC*)sub;
				kprintf("lapic, acpi id=%u apicid=%u\n", lapic->ProcessorId, lapic->Id);
				if ((lapic->LapicFlags & ACPI_MADT_ENABLED) == 0)
					continue; /* skip disabled CPU's */

				struct X86_CPU* cpu = &smp_config.cfg_cpu[cur_cpu];
				cpu->lapic_id = lapic->Id;
				cur_cpu++;
				break;
			}
			case ACPI_MADT_TYPE_IO_APIC: {
				ACPI_MADT_IO_APIC* apic = (ACPI_MADT_IO_APIC*)sub;
				kprintf("ioapic, Id=%x addr=%x base=%u\n", apic->Id, apic->Address, apic->GlobalIrqBase);

				/* Map the IOAPIC memory; the hairy details are in map_device() */
				void* ioapic_base = map_device(apic->Address);

				/* Create the associated I/O APIC and hook it up */
				struct X86_IOAPIC* ioapic = &smp_config.cfg_ioapic[cur_ioapic];
				ioapic->ioa_id = apic->Id;
				ioapic->ioa_addr = (addr_t)ioapic_base;
				ioapic_register(ioapic, apic->GlobalIrqBase);
				cur_ioapic++;
				break;
			}
			case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE: {
				ACPI_MADT_INTERRUPT_OVERRIDE* io = (ACPI_MADT_INTERRUPT_OVERRIDE*)sub;
				kprintf("intoverride, bus=%u SourceIrq=%u globalirq=%u\n", io->Bus, io->SourceIrq, io->GlobalIrq);
				KASSERT(io->SourceIrq < smp_config.cfg_num_ints, "interrupt override out of range");

				struct X86_INTERRUPT* interrupt = &smp_config.cfg_int[io->SourceIrq];
				interrupt->source_no = io->SourceIrq;
				interrupt->dest_no = io->GlobalIrq;

				/*
				 * Disable the identity mapping of this IRQ - this prevents entries from
			 	 * being overwritten.
				 */
				if (io->GlobalIrq != io->SourceIrq) {
					interrupt = &smp_config.cfg_int[io->GlobalIrq];
					interrupt->bus = NULL;
				}
				break;
			}
		}
	}

	return ANANAS_ERROR_NONE;
}

/* vim:set ts=2 sw=2: */
