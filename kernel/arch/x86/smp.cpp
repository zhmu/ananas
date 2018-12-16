#include <ananas/types.h>
#include <ananas/errno.h>
#include <ananas/util/algorithm.h>
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

namespace smp {
namespace {

constexpr size_t numberOfISAInterrupts = 16;
constexpr int irqVectorBase = 32;

template<typename Func>
void WalkMADT(const ACPI_TABLE_MADT* madt, Func func)
{
	auto sub = reinterpret_cast<const ACPI_SUBTABLE_HEADER*>(madt + 1 /* skip BSP */);
	while (sub < reinterpret_cast<const ACPI_SUBTABLE_HEADER*>((char*)madt + madt->Header.Length)) {
		func(sub);
		sub = reinterpret_cast<const ACPI_SUBTABLE_HEADER*>((char*)sub + sub->Length);
	}
}

Page* ap_page = nullptr;
volatile int can_smp_launch = 0;
int num_cpus = 0;

struct IPISource final : irq::IRQSource
{
	int GetFirstInterruptNumber() const override;
	int GetInterruptCount() const override;
	void Mask(int no) override;
	void Unmask(int no) override;
	void Acknowledge(int no) override;
} ipi_source;

int IPISource::GetFirstInterruptNumber() const
{
	return SMP_IPI_FIRST;
}

int IPISource::GetInterruptCount() const
{
	return SMP_IPI_COUNT;
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

void
DestroyAPPageTable()
{
	if (smp_ap_pages != nullptr)
		page_free(*smp_ap_pages);
	smp_ap_pages = NULL;
	smp_ap_pagedir = 0;
}

void
InitializeCPUs()
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

void InitializeLAPIC(int lapic_id)
{
	// Reset destination format to flat mode
	*reinterpret_cast<volatile uint32_t*>(lapic_base + LAPIC_DF) = 0xffffffff;
	// Ensure we are the logical destination of our local APIC
	auto v = reinterpret_cast<volatile uint32_t*>(lapic_base + LAPIC_LD);
	*v = (*v & 0x00ffffff) | 1 << (lapic_id + 24);
	// Clear Task Priority register; this enables all LAPIC interrupts
	*reinterpret_cast<volatile uint32_t*>(lapic_base + LAPIC_TPR) &= ~0xff;
	/* Finally, enable the APIC */
	*reinterpret_cast<volatile uint32_t*>(lapic_base + LAPIC_SVR) |= LAPIC_SVR_APIC_EN;

	// TODO: enable lapic using msr 0x1b just in case
}

int InitializeLAPICForBSP(const ACPI_TABLE_MADT* madt)
{
	KASSERT(madt->Address == LAPIC_BASE, "lapic base unsupported");
	lapic_base = map_device<char*>(madt->Address);
	KASSERT((addr_t)lapic_base == PTOKV(madt->Address), "mis-mapped lapic (%p != %p)", lapic_base, PTOKV(madt->Address));
	/* Fetch our local APIC ID, we need to program it shortly */
	int bsp_apic_id = (*reinterpret_cast<volatile uint32_t*>(lapic_base + LAPIC_ID)) >> 24;

	InitializeLAPIC(bsp_apic_id);
	return bsp_apic_id;
}

void InitializeIdentifyMappedPagetableForAPs()
{
	/*
	 * When booting the AP's, we need to have identity-mapped memory - and this
	 * does not exist in our normal pages. It's actually easier just to construct
	 * pagetables similar to what the multiboot stub uses to get us to long mode.
	 *
	 * Note that we can't use the kernel pagetable as it will not map the memory
	 * from where we are running at that point (maybe worth FIXME?)
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

void MapISAInterrupts(const ACPI_TABLE_MADT* madt, X86_IOAPIC& isa_ioapic, int bsp_apic_id)
{
	struct ISA_INTERRUPT {
		int i_source;
		int i_dest;
		X86_IOAPIC::Polarity i_polarity;
		X86_IOAPIC::TriggerLevel i_triggerlevel;
	};

	// Identity-map all interrupts; the MADT only contains explicit overrides
	util::array<ISA_INTERRUPT, numberOfISAInterrupts> isa_interrupts;
	for(size_t n = 0; n < isa_interrupts.size(); n++) {
		isa_interrupts[n].i_source = n;
		isa_interrupts[n].i_dest = n;
		// ISA interrupts are active-hi, edge-triggered
		isa_interrupts[n].i_polarity = X86_IOAPIC::Polarity::High;
		isa_interrupts[n].i_triggerlevel = X86_IOAPIC::TriggerLevel::Edge;
	}

	// Third walk, handle ISA interrupt overrides
	WalkMADT(madt, [&](auto sub)
	{
		if (sub->Type != ACPI_MADT_TYPE_INTERRUPT_OVERRIDE)
			return;
		auto io = reinterpret_cast<const ACPI_MADT_INTERRUPT_OVERRIDE*>(sub);
		kprintf("intoverride, SourceIrq=%u globalirq=%u flags=%x\n", io->SourceIrq, io->GlobalIrq, io->IntiFlags);
		KASSERT(io->SourceIrq < isa_interrupts.size(), "interrupt override out of range");

		auto& isa_interrupt = isa_interrupts[io->SourceIrq];
		isa_interrupt.i_source = io->SourceIrq;
		isa_interrupt.i_dest = io->GlobalIrq;
		switch(io->IntiFlags & ACPI_MADT_POLARITY_MASK) {
			default:
			case ACPI_MADT_POLARITY_CONFORMS:
				isa_interrupt.i_polarity = (io->SourceIrq == AcpiGbl_FADT.SciInterrupt) ? X86_IOAPIC::Polarity::Low : X86_IOAPIC::Polarity::High;
				break;
			case ACPI_MADT_POLARITY_ACTIVE_HIGH:
				isa_interrupt.i_polarity = X86_IOAPIC::Polarity::High;
				break;
			case ACPI_MADT_POLARITY_ACTIVE_LOW:
				isa_interrupt.i_polarity = X86_IOAPIC::Polarity::Low;
				break;
		}
		switch(io->IntiFlags & ACPI_MADT_TRIGGER_MASK) {
			default:
			case ACPI_MADT_TRIGGER_CONFORMS:
				isa_interrupt.i_triggerlevel = (io->SourceIrq == AcpiGbl_FADT.SciInterrupt) ? X86_IOAPIC::TriggerLevel::Level : X86_IOAPIC::TriggerLevel::Edge;
				break;
			case ACPI_MADT_TRIGGER_EDGE:
				isa_interrupt.i_triggerlevel = X86_IOAPIC::TriggerLevel::Edge;
				break;
			case ACPI_MADT_TRIGGER_LEVEL:
				isa_interrupt.i_triggerlevel = X86_IOAPIC::TriggerLevel::Level;
				break;
		}

		/*
		 * Disable the identity mapping of this IRQ - this prevents entries from
		 * being overwritten.
		 */
		if(io->GlobalIrq != io->SourceIrq)
			isa_interrupts[io->GlobalIrq].i_dest = -1;
	});

	/*
	 * Map the first vector as ExtInt, also known as the 'virtual interrupt
	 * wire'. We always keep it masked.
	 */
	isa_ioapic.ConfigurePin(0, 0, bsp_apic_id, X86_IOAPIC::DeliveryMode::ExtInt, X86_IOAPIC::Polarity::High, X86_IOAPIC::TriggerLevel::Edge);

	// Wire ISA interrupts to the corresponding I/O APIC
	for(const auto& isa_interrupt: isa_interrupts) {
		if (isa_interrupt.i_dest < 0)
			continue;

#if SMP_DEBUG
		kprintf("ISA: source=%u, dest=%u\n", isa_interrupt.i_source, isa_interrupt.i_dest);
#endif
		isa_ioapic.ConfigurePin(isa_interrupt.i_dest, isa_interrupt.i_source + irqVectorBase, bsp_apic_id, X86_IOAPIC::DeliveryMode::Fixed, isa_interrupt.i_polarity, isa_interrupt.i_triggerlevel);
	}
}

} // unnamed namespace

/*
 * Prepares SMP-specific memory allocations; this is separate to ensure we'll
 * have enough lower memory.
 */
void
Prepare()
{
	ap_page = page_alloc_single();
	KASSERT(ap_page->GetPhysicalAddress() < 0x100000, "ap code must be below 1MB"); /* XXX crude */
}

/*
 * Called on the Boot Strap Processor, in order to prepare the system for
 * multiprocessing.
 */
void
Init()
{
	/*
	 * The AP's start in real mode, so we need to provide them with a stub so
	 * they can run in protected mode. This stub must be located in the lower
	 * 1MB (which is why it is allocated as soon as possible in Prepare())
	 */
	{
		KASSERT(ap_page != nullptr, "Prepare() not called");
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
	int bsp_apic_id = InitializeLAPICForBSP(madt);

	// First walk, count CPU's
	num_cpus = 0;
	WalkMADT(madt, [&](auto sub)
	{
		if (sub->Type != ACPI_MADT_TYPE_LOCAL_APIC)
			return;
		auto lapic = reinterpret_cast<const ACPI_MADT_LOCAL_APIC*>(sub);
		if (lapic->LapicFlags & ACPI_MADT_ENABLED)
			num_cpus++;
	});

	// Allocate tables for the resources we found
	InitializeCPUs();

	// Second walk, map CPU's and IOAPIC's
	util::vector<X86_IOAPIC*> x86_ioapics;
	{
		int cur_cpu = 0;
		WalkMADT(madt, [&](auto sub)
		{
			switch(sub->Type) {
				case ACPI_MADT_TYPE_LOCAL_APIC: {
					auto lapic = reinterpret_cast<const ACPI_MADT_LOCAL_APIC*>(sub);
					kprintf("lapic, acpi id=%u apicid=%u\n", lapic->ProcessorId, lapic->Id);
					if ((lapic->LapicFlags & ACPI_MADT_ENABLED) == 0)
						return; /* skip disabled CPU's */

					// Fill out the LAPIC ID for each CPU; mp_stub.S will use this to
					// identify the CPU and switch to the correct stck
					auto cpu = &x86_cpus[cur_cpu];
					cpu->cpu_lapic_id = lapic->Id;
					cur_cpu++;
					break;
				}
				case ACPI_MADT_TYPE_IO_APIC: {
					ACPI_MADT_IO_APIC* apic = (ACPI_MADT_IO_APIC*)sub;
					kprintf("ioapic, Id=%x addr=%x base=%u\n", apic->Id, apic->Address, apic->GlobalIrqBase);

					// Create the associated I/O APIC and hook it up
					auto ioapic = new X86_IOAPIC(apic->Id, reinterpret_cast<addr_t>(map_device<void*>(apic->Address)), apic->GlobalIrqBase);
					x86_ioapics.push_back(ioapic);
					break;
				}
			}
		});
	}

	// Map all ISA interrupts to the BSP
	{
		auto it = util::find_if(x86_ioapics, [](const X86_IOAPIC* ioapic) {
			return ioapic->GetFirstInterruptNumber() == 0 && ioapic->GetInterruptCount() >= 15;
		});
		KASSERT(*it != nullptr, "no suitable IOAPIC found for ISA interrupts");
		auto isa_ioapic = *it;
		MapISAInterrupts(madt, *isa_ioapic, bsp_apic_id);
	}

	// Wire everything else as PCI interrupts
	for(auto& ioapic: x86_ioapics) {
		for (int vector = ioapic->GetFirstInterruptNumber(); vector < ioapic->GetFirstInterruptNumber() + ioapic->GetInterruptCount(); vector++) {
			if (vector < numberOfISAInterrupts)
				continue;

			kprintf("ioapic: routing pin %d -> irq %d\n", vector, vector + irqVectorBase);
			ioapic->ConfigurePin(vector, vector + irqVectorBase, bsp_apic_id, X86_IOAPIC::DeliveryMode::Fixed, X86_IOAPIC::Polarity::Low, X86_IOAPIC::TriggerLevel::Level);
		}
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
	for (auto& ioapic: x86_ioapics) {
		ioapic->DumpConfiguration();
		ioapic->ApplyConfiguration();
		irq::RegisterSource(*ioapic);
	}

	InitializeIdentifyMappedPagetableForAPs();
}

/*
 * Called on the Boot Strap Processor, in order to fully launch the AP's.
 */
const init::OnInit initSMPLaunch(init::SubSystem::Scheduler, init::Order::Middle, []()
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
	DestroyAPPageTable();
});

void
PanicOthers()
{
	if (num_smp_launched > 1)
		*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_EXC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_FIXED | SMP_IPI_PANIC;
}

void
BroadcastSchedule()
{
	*((volatile uint32_t*)(lapic_base + LAPIC_ICR_LO)) = LAPIC_ICR_DEST_ALL_INC_SELF | LAPIC_ICR_LEVEL_ASSERT | LAPIC_ICR_DELIVERY_FIXED | SMP_IPI_SCHEDULE;
}

} // namespace smp

/*
 * Called by mp_stub.S for every Application Processor. Should not return.
 */
extern "C" void
mp_ap_startup(uint32_t lapic_id)
{
	/* Switch to our idle thread */
	Thread* idlethread = PCPU_GET(idlethread);
  PCPU_SET(curthread, idlethread);
  scheduler::AddThread(*idlethread);

	smp::InitializeLAPIC(lapic_id);

	/* Wait for it ... */
	while (!smp::can_smp_launch)
		/* nothing */ ;

	/* We're up and running! Increment the launched count */
	__asm("lock incl (num_smp_launched)");

	/* Enable interrupts and become the idle thread; this doesn't return */
	md::interrupts::Enable();
	idle_thread(nullptr);
}

/* vim:set ts=2 sw=2: */
