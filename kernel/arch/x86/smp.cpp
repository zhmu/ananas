#include <ananas/types.h>
#include <ananas/errno.h>
#include "kernel/init.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/pcpu.h"
#include "kernel/result.h"
#include "kernel/schedule.h"
#include "kernel/thread.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel/x86/io.h"
#include "kernel/x86/acpi.h"
#include "kernel/x86/apic.h"
#include "kernel/x86/ioapic.h"
#include "kernel/x86/smp.h"
#include "kernel-md/interrupts.h"
#include "kernel-md/macro.h"
#include "kernel-md/param.h"
#include "kernel-md/vm.h"
#include "options.h"
#include "../../dev/acpi/acpica/acpi.h"

#define SMP_DEBUG 0

TRACE_SETUP;

/* Application Processor's entry point and end */
extern "C" void *__ap_entry, *__ap_entry_end;
struct X86_CPU* x86_cpus;

extern "C" char* lapic_base = nullptr;
extern "C" volatile int num_smp_launched = 1; /* BSP is always launched */
extern void* gdt;

addr_t smp_ap_pagedir;

namespace {

Page* ap_page = nullptr;
volatile int can_smp_launch = 0;
int num_cpus = 0;

struct IPISource : irq::IRQSource
{
	IPISource();

	void Mask(int no) override;
	void Unmask(int no) override;
	void Acknowledge(int no) override;
} ipi_source;

IPISource::IPISource()
	: IRQSource(SMP_IPI_FIRST, SMP_IPI_COUNT)
{
}

void IPISource::Mask(int no)
{
}

void IPISource::Unmask(int no)
{
}

void IPISource::Acknowledge(int no)
{
	X86_IOAPIC::AcknowledgeAll();
}

struct IPISchedulerHandler : irq::IHandler
{
	irq::IRQResult OnIRQ() override
	{
		/* Flip the reschedule flag of the current thread; this makes the IRQ reschedule us as needed */
		Thread* curthread = PCPU_GET(curthread);
		curthread->t_flags |= THREAD_FLAG_RESCHEDULE;
		return irq::IRQResult::Processed;
	}
} ipiSchedulerHandler;

struct IPIPanicHandler : irq::IHandler
{
	irq::IRQResult OnIRQ() override
	{
		md::interrupts::Disable();
		while (1)
			md::interrupts::Relax();

		/* NOTREACHED */
		return irq::IRQResult::Processed;
	}
} ipiPanicHandler;

template<typename T> static T
map_device(addr_t phys)
{
	return static_cast<T>(kmem_map(phys, 1, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE));
}

Page* smp_ap_pages;

static void
smp_init_ap_pagetable()
{
	/*
	 * When booting the AP's, we need to have identity-mapped memory - and this
	 * does not exist in our normal pages. It's actually easier just to construct
	 * pagetables similar to what the multiboot stub uses to get us to long mode.
	 */
	void* ptr = page_alloc_length_mapped(3 * PAGE_SIZE, smp_ap_pages, VM_FLAG_READ | VM_FLAG_WRITE);

	addr_t pa = smp_ap_pages->GetPhysicalAddress();
	uint64_t* pml4 = static_cast<uint64_t*>(ptr);
	uint64_t* pdpe = reinterpret_cast<uint64_t*>(static_cast<char*>(ptr) + PAGE_SIZE);
	uint64_t* pde = reinterpret_cast<uint64_t*>(static_cast<char*>(ptr) + PAGE_SIZE * 2);
	for (unsigned int n = 0; n < 512; n++) {
		pde[n] = (uint64_t)((addr_t)(n << 21) | PE_PS | PE_RW | PE_P);
		pdpe[n] = (uint64_t)((addr_t)(pa + PAGE_SIZE * 2) | PE_RW | PE_P);
		pml4[n] = (uint64_t)((addr_t)(pa + PAGE_SIZE) | PE_RW | PE_P);
  }

	smp_ap_pagedir = pa;
}

void
smp_destroy_ap_pagetable()
{
	if (smp_ap_pages != nullptr)
		page_free(*smp_ap_pages);
	smp_ap_pages = NULL;
	smp_ap_pagedir = 0;
}

void
smp_init_cpus()
{
	/* Prepare the CPU structure. CPU #0 is always the BSP */
	x86_cpus = new X86_CPU[num_cpus + 1];
	for (int n = 0; n < num_cpus; n++) {
		struct X86_CPU* cpu = &x86_cpus[n];

		/*
		 * Note that we don't need to allocate anything extra for the BSP.
		 */
		if (n == 0)
			continue;

		/*
		 * Allocate one buffer and place all necessary administration in there.
		 * Each AP needs an own GDT because it contains the pointer to the per-CPU
		 * data and the TSS must be distinct too.
		 */
		constexpr size_t gdtSize = GDT_NUM_ENTRIES * 16;
		auto buf = static_cast<char*>(kmalloc(gdtSize + sizeof(struct TSS) + sizeof(struct PCPU)));
		{
			cpu->cpu_gdt = buf;
			memcpy(cpu->cpu_gdt, &gdt, gdtSize);
		}
		auto tss = reinterpret_cast<struct TSS*>(buf + gdtSize);
		memset(tss, 0, sizeof(struct TSS));
		GDT_SET_TSS64(cpu->cpu_gdt, GDT_SEL_TASK, 0, (addr_t)tss, sizeof(struct TSS));
		cpu->cpu_tss = (char*)tss;

		/* Initialize per-CPU data */
		auto pcpu = reinterpret_cast<struct PCPU*>(buf + gdtSize + sizeof(struct TSS));
		memset(pcpu, 0, sizeof(struct PCPU));
		pcpu->cpuid = n;
		pcpu->tss = (addr_t)cpu->cpu_tss;
		pcpu_init(pcpu);
		cpu->cpu_pcpu = pcpu;

		/* Use the idle thread stack to execute from; we're becoming the idle thread anyway */
		cpu->cpu_stack = reinterpret_cast<char*>(pcpu->idlethread->md_rsp);
	}
}

} // unnamed namespace

/*
 * Prepares SMP-specific memory allocations; this is separate to ensure we'll
 * have enough lower memory.
 */
void
smp_prepare()
{
	ap_page = page_alloc_single();
	KASSERT(ap_page->GetPhysicalAddress() < 0x100000, "ap code must be below 1MB"); /* XXX crude */
}


/*
 * Called on the Boot Strap Processor, in order to prepare the system for
 * multiprocessing.
 */
void
smp_init()
{
	/*
	 * The AP's start in real mode, so we need to provide them with a stub so
	 * they can run in protected mode. This stub must be located in the lower
	 * 1MB (which is why it is allocated as soon as possible in smp_prepare())
	 */
	{
		KASSERT(ap_page != nullptr, "smp_prepare() not called");
		void* ap_code = kmem_map(ap_page->GetPhysicalAddress(), PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_EXECUTE);
		memcpy(ap_code, &__ap_entry, (addr_t)&__ap_entry_end - (addr_t)&__ap_entry);
		kmem_unmap(ap_code, PAGE_SIZE);
	}

	// Parse MADT table; this contains the list of CPUs and ISA interrupting
	// routing information
	ACPI_TABLE_MADT* madt;
	if (ACPI_FAILURE(AcpiGetTable(const_cast<char*>(ACPI_SIG_MADT), 0, (ACPI_TABLE_HEADER**)&madt)))
		panic("MADT table not found");

	/*
	 * The MADT doesn't tell us which CPU is the BSP, but we do know it is our
	 * current CPU, so just asking would be enough.
	 */
	KASSERT(madt->Address == LAPIC_BASE, "lapic base unsupported");
	lapic_base = map_device<char*>(madt->Address);
	KASSERT((addr_t)lapic_base == PTOKV(madt->Address), "mis-mapped lapic (%p != %p)", lapic_base, PTOKV(madt->Address));
	/* Fetch our local APIC ID, we need to program it shortly */
	int bsp_apic_id = (*(volatile uint32_t*)(lapic_base + LAPIC_ID)) >> 24;
	/* Reset destination format to flat mode */
	*(volatile uint32_t*)(lapic_base + LAPIC_DF) = 0xffffffff;
	/* Ensure we are the logical destination of our local APIC */
	volatile uint32_t* v = (volatile uint32_t*)(lapic_base + LAPIC_LD);
	*v = (*v & 0x00ffffff) | 1 << (bsp_apic_id + 24);
	/* Clear Task Priority register; this enables all LAPIC interrupts */
	*(volatile uint32_t*)(lapic_base + LAPIC_TPR) &= ~0xff;
	/* Finally, enable the APIC */
	*(volatile uint32_t*)(lapic_base + LAPIC_SVR) |= LAPIC_SVR_APIC_EN;

	// TODO: enable lapic using msr 0x1b just in case

	/* First of all, walk through the MADT and just count everything */
	num_cpus = 0;
	int num_ioapics = 0;
	for (auto sub = reinterpret_cast<ACPI_SUBTABLE_HEADER*>(madt + 1 /* skip BSP */);
	     sub < reinterpret_cast<ACPI_SUBTABLE_HEADER*>((char*)madt + madt->Header.Length);
	    sub = reinterpret_cast<ACPI_SUBTABLE_HEADER*>((char*)sub + sub->Length)) {
		switch(sub->Type) {
			case ACPI_MADT_TYPE_LOCAL_APIC: {
				ACPI_MADT_LOCAL_APIC* lapic = (ACPI_MADT_LOCAL_APIC*)sub;
				if (lapic->LapicFlags & ACPI_MADT_ENABLED)
					num_cpus++;
				break;
			}
			case ACPI_MADT_TYPE_IO_APIC:
				num_ioapics++;
				break;
		}
	}

	/*
	 * ACPI interrupt overrides will only list exceptions; this means we'll
	 * have to pre-allocate all ISA interrupts and let the overrides
	 * alter them.
	 */
	int num_ints = 16;

	// Allocate tables for the resources we found
	smp_init_cpus();
	auto x86_ioapics = new X86_IOAPIC[num_ioapics];
	auto x86_interrupts = new X86_INTERRUPT[num_ints];

	/* Identity-map all interrupts */
	for (int n = 0; n < num_ints; n++) {
		struct X86_INTERRUPT* interrupt = &x86_interrupts[n];
		interrupt->int_source_no = n;
		interrupt->int_dest_no = n;
		interrupt->int_ioapic = &x86_ioapics[0]; // XXX
	}

	int cur_cpu = 0, cur_ioapic = 0;
	for (ACPI_SUBTABLE_HEADER* sub = reinterpret_cast<ACPI_SUBTABLE_HEADER*>(madt + 1);
	     sub < (ACPI_SUBTABLE_HEADER*)((char*)madt + madt->Header.Length);
	    sub = (ACPI_SUBTABLE_HEADER*)((char*)sub + sub->Length)) {
		switch(sub->Type) {
			case ACPI_MADT_TYPE_LOCAL_APIC: {
				ACPI_MADT_LOCAL_APIC* lapic = (ACPI_MADT_LOCAL_APIC*)sub;
				kprintf("lapic, acpi id=%u apicid=%u\n", lapic->ProcessorId, lapic->Id);
				if ((lapic->LapicFlags & ACPI_MADT_ENABLED) == 0)
					continue; /* skip disabled CPU's */

				struct X86_CPU* cpu = &x86_cpus[cur_cpu];
				cpu->cpu_lapic_id = lapic->Id;
				cur_cpu++;
				break;
			}
			case ACPI_MADT_TYPE_IO_APIC: {
				ACPI_MADT_IO_APIC* apic = (ACPI_MADT_IO_APIC*)sub;
				kprintf("ioapic, Id=%x addr=%x base=%u\n", apic->Id, apic->Address, apic->GlobalIrqBase);

				/* Map the IOAPIC memory; the hairy details are in map_device() */
				void* ioapic_base = map_device<void*>(apic->Address);

				/* Create the associated I/O APIC and hook it up */
				struct X86_IOAPIC* ioapic = &x86_ioapics[cur_ioapic];
				ioapic->Initialize(apic->Id, reinterpret_cast<addr_t>(ioapic_base), apic->GlobalIrqBase);
				cur_ioapic++;
				break;
			}
			case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE: {
				ACPI_MADT_INTERRUPT_OVERRIDE* io = (ACPI_MADT_INTERRUPT_OVERRIDE*)sub;
				kprintf("intoverride, bus=%u SourceIrq=%u globalirq=%u flags=%x\n", io->Bus, io->SourceIrq, io->GlobalIrq, io->IntiFlags);
				KASSERT(io->SourceIrq < num_ints, "interrupt override out of range");

				struct X86_INTERRUPT* interrupt = &x86_interrupts[io->SourceIrq];
				interrupt->int_source_no = io->SourceIrq;
				interrupt->int_dest_no = io->GlobalIrq;
				switch(io->IntiFlags & ACPI_MADT_POLARITY_MASK) {
					default:
					case ACPI_MADT_POLARITY_CONFORMS:
						if (io->SourceIrq == AcpiGbl_FADT.SciInterrupt)
							interrupt->int_polarity = INTERRUPT_POLARITY_LOW;
						else
							interrupt->int_polarity = INTERRUPT_POLARITY_HIGH;
						break;
					case ACPI_MADT_POLARITY_ACTIVE_HIGH:
						interrupt->int_polarity = INTERRUPT_POLARITY_HIGH;
						break;
					case ACPI_MADT_POLARITY_ACTIVE_LOW:
						interrupt->int_polarity = INTERRUPT_POLARITY_LOW;
						break;
				}
				switch(io->IntiFlags & ACPI_MADT_TRIGGER_MASK) {
					default:
					case ACPI_MADT_TRIGGER_CONFORMS:
						if (io->SourceIrq == AcpiGbl_FADT.SciInterrupt)
							interrupt->int_trigger = INTERRUPT_TRIGGER_LEVEL;
						else
							interrupt->int_trigger = INTERRUPT_TRIGGER_EDGE;
						break;
					case ACPI_MADT_TRIGGER_EDGE:
						interrupt->int_trigger = INTERRUPT_TRIGGER_EDGE;
						break;
					case ACPI_MADT_TRIGGER_LEVEL:
						interrupt->int_trigger = INTERRUPT_TRIGGER_LEVEL;
						break;
				}

				/*
				 * Disable the identity mapping of this IRQ - this prevents entries from
			 	 * being overwritten.
				 */
				if(io->GlobalIrq != io->SourceIrq) {
					interrupt = &x86_interrupts[io->GlobalIrq];
					interrupt->int_ioapic = nullptr;
				}
				break;
			}
		}
	}

	/* Program the I/O APIC - we currently just wire all ISA interrupts */
	for (int i = 0; i < num_ints; i++) {
		struct X86_INTERRUPT* interrupt = &x86_interrupts[i];
#if SMP_DEBUG
		kprintf("int %u: source=%u, dest=%u, apic=%p\n",
		 i, interrupt->int_source_no, interrupt->int_dest_no, interrupt->int_ioapic);
#endif

		if (interrupt->int_ioapic == NULL)
			continue;
		if (interrupt->int_dest_no < 0)
			continue;

		/* XXX For now, route all interrupts to the BSP */
		uint32_t reg = IOREDTBL + (interrupt->int_dest_no * 2);
		uint64_t val = DESTMOD_PHYSICAL | DELMOD_FIXED | (interrupt->int_source_no + 0x20);
		if (interrupt->int_polarity == INTERRUPT_POLARITY_LOW)
			val |= INTPOL;
		if (interrupt->int_trigger == INTERRUPT_TRIGGER_LEVEL)
			val |= TRIGGER_LEVEL;
		interrupt->int_ioapic->Write(reg, val);
		interrupt->int_ioapic->Write(reg + 1, bsp_apic_id << 24);
	}

	/*
	 * Register an interrupt source for the IPI's; they appear as normal
 	 * interrupts and this lets us process them as such.
	 */
	irq::RegisterSource(ipi_source);
	if (auto result = irq::Register(SMP_IPI_PANIC, NULL, IRQ_TYPE_IPI, ipiPanicHandler); result.IsFailure())
		panic("can't register ipi");
	if (auto result = irq::Register(SMP_IPI_SCHEDULE, NULL, IRQ_TYPE_IPI, ipiSchedulerHandler); result.IsFailure())
		panic("can't register ipi");

	smp_init_ap_pagetable();
	//delete[] x86_ioapics;
	//delete[] x86_interrupts;
}

/*
 * Called on the Boot Strap Processor, in order to fully launch the AP's.
 */
static Result
smp_launch()
{
	can_smp_launch++;

	constexpr auto delay = [](int n) {
		while (n-- > 0) {
			int x = 1000;
			while (x--);
		}
	};

	/*
	 * Broadcast INIT-SIPI-SIPI-IPI to all AP's; this will wake them up and cause
	 * them to run the AP entry code.
	 */
	const auto ap_addr = ap_page->GetPhysicalAddress();
	*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_INIT;
	delay(10);
	*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_SIPI | ap_addr >> 12;
	delay(200);
	*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_SIPI | ap_addr >> 12;
	delay(200);

	kprintf("SMP: %d CPU(s) found, waiting for %d CPU(s)\n", num_cpus, num_cpus - num_smp_launched);
	while(num_smp_launched < num_cpus)
		/* wait for it ... */ ;

	/* All done - we can throw away the AP code and mappings */
	page_free(*ap_page);
	smp_destroy_ap_pagetable();

	return Result::Success();
}

INIT_FUNCTION(smp_launch, SUBSYSTEM_SCHEDULER, ORDER_MIDDLE);

void
smp_panic_others()
{
	if (num_smp_launched > 1)
		*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_FIXED | SMP_IPI_PANIC;
}

void
smp_broadcast_schedule()
{
	*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_INC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_FIXED | SMP_IPI_SCHEDULE;
}

/*
 * Called by mp_stub.S for every Application Processor. Should not return.
 */
extern "C" void
mp_ap_startup(uint32_t lapic_id)
{
	/* Switch to our idle thread */
	Thread* idlethread = PCPU_GET(idlethread);
  PCPU_SET(curthread, idlethread);
  scheduler_add_thread(*idlethread);

	/* Reset destination format to flat mode */
	*(volatile uint32_t*)(lapic_base + LAPIC_DF) = 0xffffffff;
	/* Ensure we are the logical destination of our local APIC */
	volatile uint32_t* v = (volatile uint32_t*)(lapic_base + LAPIC_LD);
	*v = (*v & 0x00ffffff) | 1 << (lapic_id + 24);
	/* Clear Task Priority register; this enables all LAPIC interrupts */
	*(volatile uint32_t*)(lapic_base + LAPIC_TPR) &= ~0xff;
	/* Finally, enable the APIC */
	*(volatile uint32_t*)(lapic_base + LAPIC_SVR) |= LAPIC_SVR_APIC_EN;

	/* Wait for it ... */
	while (!can_smp_launch)
		/* nothing */ ;

	/* We're up and running! Increment the launched count */
	__asm("lock incl (num_smp_launched)");

	/* Enable interrupts and become the idle thread; this doesn't return */
	md::interrupts::Enable();
	idle_thread(nullptr);
}

/* vim:set ts=2 sw=2: */
